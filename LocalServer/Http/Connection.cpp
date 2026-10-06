#include "Connection.hpp"
#include <utility>
#include <vector>
#include "ConnectionManager.hpp"
#include "EventStream.hpp"
#include "Server.hpp"
#include "Utils/url.h"
#include "Utils/Utils.h"


namespace HttpServer {

Connection::Connection(asio::ip::tcp::socket socket, Server *server)
  : m_socket(std::move(socket)), m_server(server)
{
}

void Connection::start() {
    doRead();
}

void Connection::stop() {
    if (m_streaming) {
        EventStream::instance().removeSubscriber(shared_from_this());
        m_streaming = false;
    }
    m_socket.close();
}

void Connection::doRead() {
    auto self(shared_from_this());

    m_socket.async_read_some(asio::buffer(m_buffer),
        [this, self](std::error_code ec, std::size_t bytes_transferred) {
        if (!ec) {
            auto result = m_requestParser.parse(
                m_request, m_buffer.data(), m_buffer.data() + bytes_transferred);
            if (result == RequestParser::GOT_HEADER) {
                m_request.uri = uriUnquote(m_request.uri.c_str());

                m_reqHandler = m_server->getRequestHandler(m_request.uri);
                if (m_reqHandler) {
                    m_state = Connection::IN_REQ_HEADER_HANDLING;
                    m_reqHandler->onRequestHeader(self);
                    if (m_response.status == Response::StatusCode::INVALID) {
                        if (m_requestParser.hasBody()) {
                            if (m_requestParser.isBodyFinished(m_request)) {
                                m_reqHandler->onRequestBody(self);
                            } else {
                                doRead();
                            }
                        }
                    }
                } else {
                    sendStockResponse(Response::NOT_FOUND);

                    // TODO: Need to handle 'keep-alive' header.
                    m_server->stopConnection(self);
                }
            } else if (result == RequestParser::GOT_BODY) {
                assert(m_reqHandler);
                m_state = Connection::IN_REQ_BODY_HANDLING;
                m_reqHandler->onRequestBody(self);
            } else if (result == RequestParser::BAD) {
                sendStockResponse(Response::StatusCode::BAD_REQUEST);

                // TODO: Need to handle 'keep-alive' header.
                m_server->stopConnection(self);
            } else {
                doRead();
            }
        } else if (ec != asio::error::operation_aborted) {
            m_server->stopConnection(self);
        }
    });
}

void Connection::send(const VecConstBuffers &buffers, bool isFinished) {
    auto self(shared_from_this());
    m_state = IN_WRITING;

    asio::async_write(m_socket, buffers,
        [this, self, isFinished](std::error_code ec, std::size_t) {
        if (!ec) {
            // 无错误发生
            if (!isFinished) {
                // 还未结束
                return;
            }

            // Initiate graceful Connection closure.
            asio::error_code ignored_ec;
            m_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);

            // TODO: 需处理 keep-alive
        }

        m_server->stopConnection(self);
    });
}

void Connection::sendResponse() {
    send(m_response.toBuffers(), true);
}

void Connection::sendStockResponse(Response::StatusCode code) {
    m_response = Response::stockResponse(code);
    send(m_response.toBuffers(), true);
}

void Connection::sendResponseHeader(int contentLength) {
    send(m_response.headerToBuffers(contentLength), false);
}

void Connection::sendResponseBody(const VecConstBuffers &buffers) {
    send(buffers, true);
}

void Connection::sendResponseBodyChunked(const std::string &data, AsioWriteCallback handler, bool finished) {
    auto self(shared_from_this());

    m_state = IN_WRITING;
    auto buffers = m_response.chunkedBodyToBuffers(data, finished);
    asio::async_write(m_socket, buffers,
                      [this, self, handler, finished](std::error_code ec, std::size_t bytesSent) {

        m_state = Connection::IN_RESPONSE_HANDLING;
        if (ec) {
            // Error occurs.
            m_server->stopConnection(self);
        } else if (finished) {
            // TODO: Need to handle 'keep-alive' header.
            // Initiate graceful Connection closure.
            asio::error_code ignored_ec;
            m_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);

            m_server->stopConnection(self);
        }

        handler(ec, bytesSent);
    });
}

void Connection::beginSse() {
    m_streaming = true;
    m_response.status = Response::OK;
    m_ssePreamble =
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "X-Accel-Buffering: no\r\n"
        "\r\n";
    m_streamQueue.push_front(m_ssePreamble);
    flushStreamQueue();
    watchClientClose();
}

void Connection::writeStream(const std::string &data) {
    if (!m_streaming || data.empty()) {
        return;
    }
    m_streamQueue.push_back(data);
    flushStreamQueue();
}

void Connection::flushStreamQueue() {
    if (m_streamWriteInFlight || m_streamQueue.empty()) {
        return;
    }
    m_streamWriteInFlight = true;
    m_streamWriteBuf = std::move(m_streamQueue.front());
    m_streamQueue.pop_front();

    auto self(shared_from_this());
    m_state = IN_WRITING;
    asio::async_write(m_socket, asio::buffer(m_streamWriteBuf),
        [this, self](std::error_code ec, std::size_t) {
        m_streamWriteInFlight = false;
        if (ec) {
            m_server->stopConnection(self);
            return;
        }
        flushStreamQueue();
    });
}

void Connection::watchClientClose() {
    doRead();
}

const std::string &Connection::stateToString(State state) {
    static std::string values[] = { "UNKOWN", "IN_READING", "IN_WRITING", "IN_REQ_HEADER_HANDLING", "IN_REQ_BODY_HANDLING", "IN_RESPONSE_HANDLING"};
    assert(state >= 0 && state <= CountOf(values));
    return values[state];
}

} // namespace HttpServer
