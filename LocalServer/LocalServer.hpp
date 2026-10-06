//
//  LocalServer.hpp
//  MusicPlayer
//
//  Created by henry_xiao on 2023/1/20.
//

#ifndef LocalServer_hpp
#define LocalServer_hpp

#include "Utils/Utils.h"
#include "Utils/Thread.h"
#include "Http/Server.hpp"
#include "WebSocket/Server.hpp"
#include "PlayerEventSender.hpp"


class LocalServer {
public:
    static LocalServer *getInstance();

    LocalServer(cstr_t address, cstr_t httpPort, cstr_t webSocketPort, cstr_t docRoot, const mbedtls_pk_context &rsaKey, const std::string &publicKey);

    void start();
    void stop();

    /** 媒体中心 HTTP 入口，如 http://127.0.0.1:12120/ */
    string getHttpBaseUrl() const;

    static void httpServerThread(void *param);
    static void webSocketServerThread(void *param);

protected:
    static LocalServer          *_instance;

    string                      m_address;
    string                      m_httpPort;

    HttpServer::Server          m_httpServer;
    WebSocket::Server           m_webSocketServer;

    PlayerEventSenderPtr        m_playerEventSender;

    CThread                     m_threadHttpServer;
    CThread                     m_threadWebSocketServer;

};

#endif /* LocalServer_hpp */
