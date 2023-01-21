//
//  LocalServer.hpp
//  Mp3Player
//
//  Created by henry_xiao on 2023/1/20.
//

#ifndef LocalServer_hpp
#define LocalServer_hpp

#include "Utils/Thread.h"
#include "Utils/Utils.h"
#include "Http/Server.hpp"
#include "WebSocket/Server.hpp"


class LocalServer {
public:
    static LocalServer *getInstance();

    LocalServer(cstr_t address, cstr_t httpPort, cstr_t webSocketPort, cstr_t docRoot);

    void start();
    void stop();

    static void httpServerThread(void *param);
    static void webSocketServerThread(void *param);

protected:
    static LocalServer          *_instance;

    HttpServer::Server          m_httpServer;
    WebSocket::Server           m_webSocketServer;

    CThread                     m_threadHttpServer;
    CThread                     m_threadWebSocketServer;

};

#endif /* LocalServer_hpp */
