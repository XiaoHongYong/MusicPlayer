//
//  HttpConnection.cpp
//

#include "HttpConnection.hpp"
#include <utility>
#include <vector>
#include "HttpServer.hpp"
#include "IHttpRequestHandler.hpp"
#include "../UvAsync.hpp"
#include "../../Utils/url.h"


static http_parser_settings _parserSettings;
static StringView KeepAlive("Keep-Alive");

struct UvConnectionData {
    UvConnectionData(const HttpConnectionPtr &conn) : connection(conn) { }
    HttpConnectionPtr           connection;
};

struct UvWriteCtx : public uv_write_t {
    ConnWriteCallback           callback;
};

static UvConnectionData *getUvConnectionData(uv_stream_t *stream) {
    return (UvConnectionData *)stream->data;
}

void initHttpReqParserSettings() {
    _parserSettings.on_url = HttpConnection::onUrlCb;
    _parserSettings.on_header_field = HttpConnection::onHeaderFieldCb;
    _parserSettings.on_header_value = HttpConnection::onHeaderValueCb;
    _parserSettings.on_headers_complete = HttpConnection::onHeadersCompleteCb;
    _parserSettings.on_body = HttpConnection::onBodyCb;
    _parserSettings.on_message_complete = HttpConnection::onMessageCompleteCb;
}

static void close_cb(uv_handle_t *handle) {
    auto data = getUvConnectionData((uv_stream_t *)handle);
    data->connection = nullptr;
}

static void shutdown_cb(uv_shutdown_t *shutdown_req, int status) {
    uv_close((uv_handle_t *)shutdown_req->handle, close_cb);
    delete shutdown_req;
}

void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = (char *)malloc(suggested_size);
    buf->len = suggested_size;
    assert(buf->base != NULL);
}

void write_cb(uv_write_t *write_req, int status) {
    UvWriteCtx *writeCtx = (UvWriteCtx *)write_req;

    if (writeCtx->callback) {
        writeCtx->callback(status);
    }
    delete writeCtx;
}

HttpConnection::HttpConnection(HttpServer &server) : _server(server), _response(this) {
    memset(&_uv_tcp, 0, sizeof(_uv_tcp));

    reset();
}

HttpConnection::~HttpConnection() {
    delete getUvConnectionData((uv_stream_t *)&_uv_tcp);
}

void HttpConnection::reset() {
    _reqHandler = nullptr;
    _request.methodID = METHOD_INVALID;
    _request.body.clear();
    _request.headers.clear();
    _request.uri.clear();
    _request.versionMajor = _request.versionMinor = 0;

    _headerName.clear();
    _headerValue.clear();
    _keepAlive = false;

    _response.body.clear();
    _response.headers.clear();
    _response.statusCode = HttpStatusCode::INVALID;

    _bodyTooLarge = false;

    http_parser_init(&_httpParser, HTTP_REQUEST);

    _status = UNKOWN;
}

void HttpConnection::start() {
    if (!_uv_tcp.data) {
        _uv_tcp.data = new UvConnectionData(shared_from_this());
    }

    http_parser_init(&_httpParser, HTTP_REQUEST);
    _httpParser.data = this;
    _uv_tcp.close_cb = close_cb;
    int r = uv_read_start(uv_stream_handle(), alloc_cb, HttpConnection::readCallback);
    assert(r == 0);
}

void HttpConnection::stop() {
    uv_shutdown_t *shutdown_req = new uv_shutdown_t;
    uv_shutdown(shutdown_req, uv_stream_handle(), shutdown_cb);
}

void HttpConnection::stopConnection() {
    _server.stopConnection(shared_from_this());
}

void HttpConnection::readCallback(uv_stream_t *handle, ssize_t nread, const uv_buf_t *buf) {
    auto data = getUvConnectionData(handle);
    HttpConnection *conn = data->connection.get();
    conn->_status = HttpConnection::IN_READING;

    DLOG(INFO).write(buf->base, nread);

    if (nread >= 0) {
        http_parser_execute(&conn->_httpParser, &_parserSettings, buf->base, nread);
    } else {
        if (nread == UV_EOF) {
            // do nothing
        } else {
            // LOGF("read: %s\n", uv_strerror(nread));
        }

        conn->stopConnection();
    }

    free(buf->base);
}

int HttpConnection::onUrlCb(http_parser *parser, const char *at, size_t length) {
    HttpConnection *conn = (HttpConnection *)parser->data;

    conn->_request.uri.append(at, length);

    return 0;
}

int HttpConnection::onHeaderFieldCb(http_parser *parser, const char *at, size_t length) {
    HttpConnection *conn = (HttpConnection *)parser->data;

    if (conn->_hasValue) {
        conn->_request.headers.push_back({ conn->_headerName, conn->_headerValue });
        conn->_headerName.clear();
        conn->_headerValue.clear();
        conn->_hasValue = false;
    }

    conn->_headerName.append(at, length);

    return 0;
}

int HttpConnection::onHeaderValueCb(http_parser *parser, const char *at, size_t length) {
    HttpConnection *conn = (HttpConnection *)parser->data;

    conn->_hasValue = true;
    conn->_headerValue.append(at, length);

    return 0;
}

int HttpConnection::onHeadersCompleteCb(http_parser *parser) {
    HttpConnection *conn = (HttpConnection *)parser->data;

    if (conn->_hasValue) {
        conn->_request.headers.push_back({ conn->_headerName, conn->_headerValue });
        conn->_headerName.clear();
        conn->_headerValue.clear();
        conn->_hasValue = false;
    }

    auto keepAlive = getHeaderByName(conn->_request.headers, HEADER_CONNECTION);
    conn->_keepAlive = keepAlive && KeepAlive.iEqual(*keepAlive);

    conn->_request.methodID = (HttpMethod)parser->method;
    conn->_request.versionMajor = parser->http_major;
    conn->_request.versionMinor = parser->http_minor;
    conn->_request.uri = uriUnquote(conn->_request.uri.c_str());
    conn->_status = HttpConnection::IN_REQ_BODY_HANDLING;
    conn->_reqHandler = conn->_server.getRequestHandler(conn->_request.uri).get();
    conn->_status = HttpConnection::IN_REQ_HEADER_HANDLING;
    conn->_reqHandler->onRequestHeader(conn->shared_from_this());

    DLOG(INFO) << "onRequestHeader: " << conn->_request.uri << ", method: " << (HttpMethod)parser->method;

    return 0;
}

int HttpConnection::onBodyCb(http_parser *parser, const char *at, size_t length) {
    HttpConnection *conn = (HttpConnection *)parser->data;
    conn->_status = HttpConnection::IN_REQ_BODY_HANDLING;

    // 限制请求体大小，防止内存被无限制撑大。
    if (conn->_request.body.size() + length > HttpConnection::MAX_BODY_SIZE) {
        conn->_bodyTooLarge = true;
        return 0;
    }

    conn->_request.body.append(at, length);

    return 0;
}

int HttpConnection::onMessageCompleteCb(http_parser *parser) {
    HttpConnection *conn = (HttpConnection *)parser->data;

    if (conn->_bodyTooLarge) {
        // 拒绝超大的请求体，直接回 413，不再交给业务 handler。
        auto &response = conn->_response;
        response.statusCode = HttpStatusCode::PAYLOAD_TOO_LARGE;
        response.body = "413 Payload Too Large";
        response.sendAll();
        return 1;
    }

    if (conn->_response.statusCode == HttpStatusCode::INVALID) {
        DLOG(INFO) << "onRequestBody: " << conn->_request.uri << ", method: " << (HttpMethod)parser->method;
        conn->_status = HttpConnection::IN_RESPONSE_HANDLING;
        conn->_reqHandler->onRequestBody(conn->shared_from_this());
    }

    return 1;
}

int HttpConnection::send(const VecConstBuffers &buffers, bool finished) {
    UvAsync::sendAsyncCallback(_server.loop(), [this, buffers, finished]() {
        UvWriteCtx *write_req = new UvWriteCtx;

        if (finished) {
            if (this->_keepAlive) {
                write_req->callback = [this](int err) {
                    this->reset();
                    this->_status = IN_KEEP_ALIVE;
                };
            } else {
                write_req->callback = [this](int err) {
                    this->stopConnection();
                };
            }
        }

        uv_buf_t *bufs = (uv_buf_t *)alloca(sizeof(uv_buf_t) * buffers.size());
        for (int i = 0; i < (int)buffers.size(); i++) {
            auto &s = buffers[i];
            bufs[i] = { s.data, s.len };
        }

        uv_write(write_req, uv_stream_handle(), bufs, (int)buffers.size(), write_cb);
    });

    return ERR_OK;
}

int HttpConnection::send(const VecConstBuffers &buffers, ConnWriteCallback callback, bool finished) {
    UvAsync::sendAsyncCallback(_server.loop(), [this, buffers, finished, callback]() {
        UvWriteCtx *write_req = new UvWriteCtx;

        if (finished) {
            if (this->_keepAlive) {
                write_req->callback = [this, callback](int err) {
                    callback(0);
                    this->reset();
                    this->_status = IN_KEEP_ALIVE;
                };
            } else {
                write_req->callback = [this, callback](int err) {
                    callback(0);
                    this->stopConnection();
                };
            }
        } else {
            write_req->callback = callback;
        }

        uv_buf_t *bufs = (uv_buf_t *)alloca(sizeof(uv_buf_t) * buffers.size());
        for (int i = 0; i < (int)buffers.size(); i++) {
            auto &s = buffers[i];
            bufs[i] = { s.data, s.len };
        }

        uv_write(write_req, uv_stream_handle(), bufs, (int)buffers.size(), write_cb);
    });

    return ERR_OK;
}

string HttpConnection::statusToString(ConnectionStatus status) {
    static string values[] = { "UNKOWN", "IN_READING", "IN_WRITING", "IN_REQ_HEADER_HANDLING", "IN_REQ_BODY_HANDLING", "IN_RESPONSE_HANDLING"};
    assert(status >= 0 && status <= CountOf(values));
    return values[status];
}

void HttpConnection::dumpStatus(IJsonWriter *writer) {
    writer->startObject();

    writer->writePropString("status", statusToString(_status));

    writer->endObject();
}
