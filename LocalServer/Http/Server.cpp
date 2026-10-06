#include "Server.hpp"
#include "Utils/Utils.h"
#include <signal.h>
#include <utility>


namespace HttpServer {

Server::Server(const std::string &address, const std::string &port)
    : m_address(address), m_port(port), m_ioContext(1), m_signals(m_ioContext),
      m_acceptor(m_ioContext), m_connectionManager()
{
    m_signals.add(SIGINT);
    m_signals.add(SIGTERM);
#if defined(SIGQUIT)
    m_signals.add(SIGQUIT);
#endif // defined(SIGQUIT)

    doAwaitStop();
    // bind/listen 放到 run()：端口占用等错误应在工作线程里捕获，不能拖垮主线程启动。
}

void Server::openAcceptor() {
    // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
    asio::ip::tcp::resolver resolver(m_ioContext);
    asio::ip::tcp::endpoint endpoint = *resolver.resolve(m_address, m_port).begin();
    m_acceptor.open(endpoint.protocol());
    m_acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    m_acceptor.bind(endpoint);
    m_acceptor.listen();

    doAccept();
}

void Server::run() {
    openAcceptor();

    // The io_context::run() call will block until all asynchronous operations
    // have finished. While the Server is running, there is always at least one
    // asynchronous operation outstanding: the asynchronous accept call waiting
    // for new incoming connections.
    m_ioContext.run();
}

void Server::stopConnection(const ConnectionPtr &connection) {
    m_connectionManager.stop(connection);
}

IRequestHandlerPtr Server::getRequestHandler(const std::string &url) {
    for (auto &handler : m_reqHandlers) {
        if (iStartsWith(url.c_str(), handler->getUriPath().c_str())) {
            return handler;
        }
    }

    return nullptr;
}

void Server::registerRequestHandler(const IRequestHandlerPtr &handler) {
    m_reqHandlers.push_back(handler);
}

void Server::doAccept() {
    m_acceptor.async_accept(
        [this](std::error_code ec, asio::ip::tcp::socket socket) {
        // Check whether the Server was stopped by a signal before this
        // completion handler had a chance to run.
        if (!m_acceptor.is_open()) {
            return;
        }

        if (!ec) {
            m_connectionManager.start(std::make_shared<Connection>(std::move(socket), this));
        }

        doAccept();
    });
}

void Server::doAwaitStop() {
    m_signals.async_wait([this](std::error_code /*ec*/, int /*signo*/) {
        // The Server is stopped by cancelling all outstanding asynchronous
        // operations. Once all operations have finished the io_context::run()
        // call will exit.
        m_acceptor.close();
        m_connectionManager.stopAll();
    });
}

} // namespace HttpServer
