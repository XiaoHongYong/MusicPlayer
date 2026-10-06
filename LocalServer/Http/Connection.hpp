#ifndef HTTP_CONNECTION_HPP
#define HTTP_CONNECTION_HPP

#include <array>
#include <deque>
#include <memory>
#include <string>
#include <asio.hpp>
#include "Response.hpp"
#include "Request.hpp"
#include "IRequestHandler.hpp"
#include "RequestParser.hpp"


namespace HttpServer {

class ConnectionManager;
class Server;

using AsioWriteCallback = std::function<void (asio::error_code, std::size_t)>;

class Connection : public std::enable_shared_from_this<Connection> {
public:
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    explicit Connection(asio::ip::tcp::socket socket, Server *server);

    void start();
    void stop();

    Request &request() { return m_request; }
    Response &response() { return m_response; }

    void sendResponseHeader(int contentLength = -1);
    void sendResponseBody(const VecConstBuffers &buffers);
    void sendResponseBodyChunked(const std::string &data, AsioWriteCallback handler, bool finished);
    void sendResponseBody(const std::string &body) {
        VecConstBuffers buffers;
        buffers.push_back(asio::buffer(body));
        sendResponseBody(buffers);
    }

    void sendResponse();
    void sendStockResponse(Response::StatusCode code);

    /** SSE：发送无 Content-Length 的 200 头并保持连接. */
    void beginSse();
    void writeStream(const std::string &data);
    bool isStreaming() const { return m_streaming; }

private:
    void stopConnection();

    void reset();

    void doRead();
    void send(const VecConstBuffers &buffers, bool isFinished);

    enum State {
        UNKOWN,
        IN_READING,
        IN_WRITING,
        IN_REQ_HEADER_HANDLING,
        IN_REQ_BODY_HANDLING,
        IN_RESPONSE_HANDLING,
    };

    static const std::string &stateToString(State status);

    asio::ip::tcp::socket       m_socket;

    Server                      *m_server;

    IRequestHandlerPtr          m_reqHandler;

    std::array<char, 8192>      m_buffer;

    Request                     m_request;

    RequestParser               m_requestParser;

    Response                    m_response;

    RequestParser::ResultType   m_requestResult;
    State                       m_state;

    bool                        m_streaming = false;
    bool                        m_streamWriteInFlight = false;
    std::string                 m_ssePreamble;
    std::string                 m_streamWriteBuf;
    std::deque<std::string>     m_streamQueue;

    void flushStreamQueue();
    void watchClientClose();

};

using ConnectionPtr = std::shared_ptr<Connection>;

} // namespace HttpServer

#endif // HTTP_CONNECTION_HPP
