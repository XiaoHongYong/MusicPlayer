#include <string>
#include "Response.hpp"
#include "Connection.hpp"
#include "Utils/Utils.h"


namespace HttpServer {

const std::string HEADER_CONTENT_TYPE = "Content-Type";
const std::string HEADER_CONTENT_LENGTH = "Content-Length";

std::string *getHeaderByName(VecHttpHeaders &headers, const std::string &name) {
    for (auto &header : headers) {
        if (header.name == name) {
            return &header.value;
        }
    }

    return nullptr;
}

const std::string &getStatusLine(Response::StatusCode code) {
    static const std::string ok = "HTTP/1.0 200 OK\r\n";
    static const std::string created = "HTTP/1.0 201 Created\r\n";
    static const std::string accepted = "HTTP/1.0 202 Accepted\r\n";
    static const std::string no_content = "HTTP/1.0 204 No Content\r\n";
    static const std::string multiple_choices = "HTTP/1.0 300 Multiple Choices\r\n";
    static const std::string moved_permanently = "HTTP/1.0 301 Moved Permanently\r\n";
    static const std::string moved_temporarily = "HTTP/1.0 302 Moved Temporarily\r\n";
    static const std::string not_modified = "HTTP/1.0 304 Not Modified\r\n";
    static const std::string partial_content = "HTTP/1.0 206 Partial Content\r\n";
    static const std::string bad_request = "HTTP/1.0 400 Bad Request\r\n";
    static const std::string unauthorized = "HTTP/1.0 401 Unauthorized\r\n";
    static const std::string forbidden = "HTTP/1.0 403 Forbidden\r\n";
    static const std::string not_found = "HTTP/1.0 404 Not Found\r\n";
    static const std::string conflict = "HTTP/1.0 409 Conflict\r\n";
    static const std::string internal_server_error = "HTTP/1.0 500 Internal Server Error\r\n";
    static const std::string not_implemented = "HTTP/1.0 501 Not Implemented\r\n";
    static const std::string bad_gateway = "HTTP/1.0 502 Bad Gateway\r\n";
    static const std::string service_unavailable = "HTTP/1.0 503 Service Unavailable\r\n";

    switch (code) {
        case Response::StatusCode::OK: return ok;
        case Response::StatusCode::CREATED: return created;
        case Response::StatusCode::ACCEPTED: return accepted;
        case Response::StatusCode::NO_CONTENT: return no_content;
        case Response::StatusCode::MULTIPLE_CHOICES: return multiple_choices;
        case Response::StatusCode::MOVED_PERMANENTLY: return moved_permanently;
        case Response::StatusCode::MOVED_TEMPORARILY: return moved_temporarily;
        case Response::StatusCode::NOT_MODIFIED: return not_modified;
        case Response::StatusCode::PARTIAL_CONTENT: return partial_content;
        case Response::StatusCode::BAD_REQUEST: return bad_request;
        case Response::StatusCode::UNAUTHORIZED: return unauthorized;
        case Response::StatusCode::FORBIDDEN: return forbidden;
        case Response::StatusCode::NOT_FOUND: return not_found;
        case Response::StatusCode::CONFLICT: return conflict;
        case Response::StatusCode::INTERNAL_SERVER_ERROR: return internal_server_error;
        case Response::StatusCode::NOT_IMPLEMENTED: return not_implemented;
        case Response::StatusCode::BAD_GATEWAY: return bad_gateway;
        case Response::StatusCode::SERVICE_UNAVAILABLE: return service_unavailable;

        default: assert(0); return internal_server_error;
    }
}

static const char name_value_separator[] = {':', ' '};
static const char crlf[] = {'\r', '\n'};

Response::Response() {
    status = INVALID;
}

VecConstBuffers Response::headerToBuffers(int bodyLength) {
    static const std::string NAME_VALUE_SEP = ": ";
    static const std::string CRLF = "\r\n";
    static const std::string HEADER_TRANSFER_ENCODING = "Transfer-Encoding";
    static const std::string ENCODING_CHUNKED = "chunked";

    VecConstBuffers buffers;

    buffers.push_back(asio::buffer(getStatusLine(status)));

    if (bodyLength == -1) {
        headers.insert(headers.begin(), {HEADER_TRANSFER_ENCODING, ENCODING_CHUNKED});
    } else {
        headers.insert(headers.begin(), {HEADER_CONTENT_LENGTH, std::to_string(bodyLength)});
    }

    if (getHeaderByName(headers, HEADER_CONTENT_TYPE) == nullptr) {
        headers.insert(headers.end(), {HEADER_CONTENT_TYPE, "text/plain"});
    }

    for (auto &header : headers) {
        buffers.push_back(asio::buffer(header.name));
        buffers.push_back(asio::buffer(NAME_VALUE_SEP));
        buffers.push_back(asio::buffer(header.value));
        buffers.push_back(asio::buffer(CRLF));
    }

    buffers.push_back(asio::buffer(CRLF));

    return buffers;
}

VecConstBuffers Response::chunkedBodyToBuffers(const std::string &body, bool isFinished) {
    static const std::string CRLF = "\r\n";
    static const std::string CHUNKED_END_CRLF = "0\r\n\r\n";

    VecConstBuffers buffers;

    // Need to save length to headers, to prevent being recycled.
    headers.push_back({"", stringPrintf("%x\r\n", body.size())});
    buffers.push_back(asio::buffer(headers.back().value));
    buffers.push_back(asio::buffer(body));
    buffers.push_back(asio::buffer(CRLF));

    if (isFinished) {
        buffers.push_back(asio::buffer(CHUNKED_END_CRLF));
    }

    return buffers;
}

void Response::addHeader(const string &name, const string &value) {
    headers.push_back({name, value});
}

VecConstBuffers Response::toBuffers() {
    VecConstBuffers buffers;

    buffers.push_back(asio::buffer(getStatusLine(status)));

    for (std::size_t i = 0; i < headers.size(); ++i) {
        Header &h = headers[i];
        buffers.push_back(asio::buffer(h.name));
        buffers.push_back(asio::buffer(name_value_separator));
        buffers.push_back(asio::buffer(h.value));
        buffers.push_back(asio::buffer(crlf));
    }

    buffers.push_back(asio::buffer(crlf));
    buffers.push_back(asio::buffer(body));

    return buffers;
}

std::string formatResponseBody(const std::string &title) {
    return "<html>" "<head><title>" + title + "</title></head>"
        "<body><h1>" + title + "</h1></body>" "</html>";
}

std::string statusCodeToContent(Response::StatusCode code) {
    static const std::string ok = "";
    static const std::string created = formatResponseBody("201 Created");
    static const std::string accepted = formatResponseBody("202 Accepted");
    static const std::string no_content = formatResponseBody("204 Content");
    static const std::string multiple_choices = formatResponseBody("300 Multiple Choices");
    static const std::string moved_permanently = formatResponseBody("301 Moved Permanently");
    static const std::string moved_temporarily = formatResponseBody("302 Moved Temporarily");
    static const std::string not_modified = formatResponseBody("304 Not Modified");
    static const std::string partial_content = formatResponseBody("206 Partial Content");
    static const std::string bad_request = formatResponseBody("400 Bad Request");
    static const std::string unauthorized = formatResponseBody("401 Unauthorized");
    static const std::string forbidden = formatResponseBody("403 Forbidden");
    static const std::string not_found = formatResponseBody("404 Not Found");
    static const std::string conflict = formatResponseBody("409 Conflict");
    static const std::string internal_server_error = formatResponseBody("500 Internal Server Error");
    static const std::string not_implemented = formatResponseBody("501 Not Implemented");
    static const std::string bad_gateway = formatResponseBody("502 Bad Gateway");
    static const std::string service_unavailable = formatResponseBody("503 Service Unavailable");

    switch (code) {
        case Response::StatusCode::OK: return ok;
        case Response::StatusCode::CREATED: return created;
        case Response::StatusCode::ACCEPTED: return accepted;
        case Response::StatusCode::NO_CONTENT: return no_content;
        case Response::StatusCode::MULTIPLE_CHOICES: return multiple_choices;
        case Response::StatusCode::MOVED_PERMANENTLY: return moved_permanently;
        case Response::StatusCode::MOVED_TEMPORARILY: return moved_temporarily;
        case Response::StatusCode::NOT_MODIFIED: return not_modified;
        case Response::StatusCode::PARTIAL_CONTENT: return partial_content;
        case Response::StatusCode::BAD_REQUEST: return bad_request;
        case Response::StatusCode::UNAUTHORIZED: return unauthorized;
        case Response::StatusCode::FORBIDDEN: return forbidden;
        case Response::StatusCode::NOT_FOUND: return not_found;
        case Response::StatusCode::CONFLICT: return conflict;
        case Response::StatusCode::INTERNAL_SERVER_ERROR: return internal_server_error;
        case Response::StatusCode::NOT_IMPLEMENTED: return not_implemented;
        case Response::StatusCode::BAD_GATEWAY: return bad_gateway;
        case Response::StatusCode::SERVICE_UNAVAILABLE: return service_unavailable;

        default: assert(0); return internal_server_error;
    }
}

Response Response::stockResponse(Response::StatusCode status) {
    Response rep;
    rep.status = status;
    rep.body = statusCodeToContent(status);
    rep.headers.resize(2);
    rep.headers[0].name = "Content-Length";
    rep.headers[0].value = std::to_string(rep.body.size());
    rep.headers[1].name = "Content-Type";
    rep.headers[1].value = "text/html";
    return rep;
}

} // namespace HttpServer
