
#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <asio.hpp>
#include <string>
#include "Connection.hpp"
#include "ConnectionManager.hpp"
#include "IRequestHandler.hpp"


namespace HttpServer {

class Server {
public:
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    explicit Server(const std::string &address, const std::string &port);

    void run();

    /// 主动停止：close acceptor、停掉所有连接并 stop io_context，使 run() 返回。
    /// 主线程退出前调用，避免在全局对象(如 g_player)析构后 HTTP 工作线程仍访问它们。
    void stop();

    IRequestHandlerPtr getRequestHandler(const std::string &url);

    void registerRequestHandler(const IRequestHandlerPtr &handler);

    void stopConnection(const ConnectionPtr &connection);

    asio::io_context &ioContext() { return m_ioContext; }

private:
    /// 打开 acceptor 并 bind/listen。失败时抛 std::system_error（由工作线程捕获）。
    void openAcceptor();

    void doAccept();

    void doAwaitStop();

    std::string                     m_address;
    std::string                     m_port;

    asio::io_context                m_ioContext;

    /// The signal_set is used to register for process termination notifications.
    asio::signal_set                m_signals;

    /// Acceptor used to listen for incoming connections.
    asio::ip::tcp::acceptor         m_acceptor;

    /// The connection manager which owns all live connections.
    ConnectionManager               m_connectionManager;

    VecRequestHandler               m_reqHandlers;

};

} // namespace HttpServer

#endif
