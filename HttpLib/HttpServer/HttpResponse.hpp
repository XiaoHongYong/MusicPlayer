//
//  HttpResponse.hpp
//

#pragma once

#ifndef HttpResponse_hpp
#define HttpResponse_hpp

#include <functional>
#include "HttpRequest.hpp"


enum HttpStatusCode {
    INVALID                             = 0,
    OK                                  = 200,
    CREATED                             = 201,
    ACCEPTED                            = 202,
    NO_CONTENT                          = 204,
    MULTIPLE_CHOICES                    = 300,
    MOVED_PERMANENTLY                   = 301,
    MOVED_TEMPORARILY                   = 302,
    NOT_MODIFIED                        = 304,
    BAD_REQUEST                         = 400,
    UNAUTHORIZED                        = 401,
    FORBIDDEN                           = 403,
    NOT_FOUND                           = 404,
    PAYLOAD_TOO_LARGE                   = 413,
    INTERNAL_SERVER_ERROR               = 500,
    NOT_IMPLEMENTED                     = 501,
    BAD_GATEWAY                         = 502,
    SERVICE_UNAVAILABLE                 = 503,
};

extern const string HEADER_CONTENT_TYPE;
extern const string HEADER_CONTENT_LENGTH;
extern const string HEADER_CONNECTION;

typedef std::vector<StringView> VecConstBuffers;
typedef std::function<void (int err)> ConnWriteCallback;

class HttpResponse {
public:
    HttpResponse(HttpConnection *connection);
    virtual ~HttpResponse() {}

    int sendAll();
    int sendAll(const VecConstBuffers &body);

    int sendHeader();
    void sendBody(const VecConstBuffers &buffers, ConnWriteCallback handler);
    void sendBodyChunked(const string &body, ConnWriteCallback callback, bool isFinished);

    void addHeader(const string &name, const string &value);

private:
    VecConstBuffers headerToBuffers(int bodyLength);

    HttpConnection                      *_connection;

public:
    HttpStatusCode                      statusCode;
    ListHttpHeaders                     headers;

    string                              body;
};


#endif /* HttpResponse_hpp */
