//
//  HttpConnection.hpp
//

#ifndef HTTP_Connection_HPP
#define HTTP_Connection_HPP

#include <array>
#include <memory>
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "../../Utils/IJsonWriter.hpp"
#include "uv.h"
#include "http_parser.h"


class HttpServer;
class ConnectionManager;
class IHttpRequestHandler;

void initHttpReqParserSettings();

class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
public:
    HttpConnection(const HttpConnection&) = delete;
    HttpConnection& operator=(const HttpConnection&) = delete;

    HttpConnection(HttpServer &server);
    virtual ~HttpConnection();

    HttpRequest &request() { return _request; }
    HttpResponse &response() { return _response; }

    int send(const VecConstBuffers &buffers, bool finished);
    int send(const VecConstBuffers &buffers, ConnWriteCallback handler, bool finished);

    void dumpStatus(IJsonWriter *writer);

    uv_tcp_t *uv_tcp_handle() { return &_uv_tcp; }
    uv_stream_t *uv_stream_handle() { return (uv_stream_t *)&_uv_tcp; }

    bool isKeepAlive() const { return _keepAlive; }

    static int onUrlCb(http_parser *parser, const char *at, size_t length);
    static int onHeaderFieldCb(http_parser *parser, const char *at, size_t length);
    static int onHeaderValueCb(http_parser *parser, const char *at, size_t length);
    static int onHeadersCompleteCb(http_parser *parser);
    static int onBodyCb(http_parser *parser, const char *at, size_t length);
    static int onMessageCompleteCb(http_parser *parser);

protected:
    void start();
    void stop();

    friend class HttpServer;

private:
    static void readCallback(uv_stream_t *handle, ssize_t nread, const uv_buf_t *buf);
    void stopConnection();

    void reset();

    void doRead();

private:
    enum ConnectionStatus {
        UNKOWN,
        IN_READING,
        IN_WRITING,
        IN_REQ_HEADER_HANDLING,
        IN_REQ_BODY_HANDLING,
        IN_RESPONSE_HANDLING,
        IN_KEEP_ALIVE,
    };

    static string statusToString(ConnectionStatus status);

    HttpServer                                      &_server;
    uv_tcp_t                                        _uv_tcp;
    http_parser                                     _httpParser;

    HttpRequest                                     _request;
    IHttpRequestHandler                             *_reqHandler;

    HttpResponse                                    _response;

    ConnectionStatus                                _status;
    bool                                            _keepAlive = false;

    string                                          _headerName, _headerValue;
    bool                                            _hasValue = false;

    // 请求体大小上限，防止无限 buffering 的内存 DoS。
    static const size_t                             MAX_BODY_SIZE = 8 * 1024 * 1024;
    bool                                            _bodyTooLarge = false;

};

typedef std::shared_ptr<HttpConnection>     HttpConnectionPtr;

#endif  // HTTP_Connection_HPP
