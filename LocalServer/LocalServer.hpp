//
//  LocalServer.hpp
//  MusicPlayer
//

#ifndef LocalServer_hpp
#define LocalServer_hpp

#include "Utils/Utils.h"
#include "Utils/Thread.h"
#include "Http/Server.hpp"
#include "PlayerEventSender.hpp"


class LocalServer {
public:
    static LocalServer *getInstance();

    LocalServer(cstr_t address, cstr_t httpPort, cstr_t docRoot);

    void start();
    void stop();

    /** 媒体中心 HTTP 入口，如 http://127.0.0.1:12120/ */
    string getHttpBaseUrl() const;

    static void httpServerThread(void *param);

protected:
    static LocalServer          *_instance;

    string                      m_address;
    string                      m_httpPort;

    HttpServer::Server          m_httpServer;

    PlayerEventSenderPtr        m_playerEventSender;

    CThread                     m_threadHttpServer;

};

#endif /* LocalServer_hpp */
