#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <string>
#include <vector>
#include <asio.hpp>
#include "Request.hpp"


namespace HttpServer {

extern const std::string HEADER_CONTENT_TYPE;
extern const std::string HEADER_CONTENT_LENGTH;

class Connection;

using VecConstBuffers = std::vector<asio::const_buffer>;

class Response {
public:
    enum StatusCode {
        INVALID                     = 0,
        OK                          = 200,
        CREATED                     = 201,
        ACCEPTED                    = 202,
        NO_CONTENT                  = 204,
        MULTIPLE_CHOICES            = 300,
        MOVED_PERMANENTLY           = 301,
        MOVED_TEMPORARILY           = 302,
        NOT_MODIFIED                = 304,
        PARTIAL_CONTENT             = 206,
        BAD_REQUEST                 = 400,
        UNAUTHORIZED                = 401,
        FORBIDDEN                   = 403,
        NOT_FOUND                   = 404,
        CONFLICT                    = 409,
        INTERNAL_SERVER_ERROR       = 500,
        NOT_IMPLEMENTED             = 501,
        BAD_GATEWAY                 = 502,
        SERVICE_UNAVAILABLE         = 503,
    };

    Response();
    virtual ~Response() {}

    void addHeader(const std::string &name, const std::string &value);

    VecConstBuffers toBuffers();
    VecConstBuffers headerToBuffers(int bodyLength);
    VecConstBuffers chunkedBodyToBuffers(const std::string &body, bool isFinished);

    static Response stockResponse(Response::StatusCode status);

public:
    StatusCode                  status;
    std::vector<Header>         headers;

    std::string                 body;
};

} // namespace HttpServer

#endif
