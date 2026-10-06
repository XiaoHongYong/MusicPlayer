//
//  LocalServer.cpp
//  MusicPlayer
//

#include "LocalServer.hpp"
#include "Http/StaticFilesHandler.hpp"
#include "Http/ApiHandler.hpp"
#include "Http/EventStream.hpp"
#include "MPlayer/MediaScanner.h"
#include "Utils/rapidjson.h"
#include "../Skin/SkinTypes.h"
#include "../MPlayer/Player.h"


LocalServer *LocalServer::_instance = nullptr;

string getLocal3WDir() {
    string path = g_profile.getString("LocalWWW", "");
    if (!path.empty()) {
        if (isDirExist(path.c_str())) {
            return path;
        }
    }

    path = getAppResourceDir();
    path += "local-server";
    path += PATH_SEP_STR;
    return path;
}

static string jsonScanPayload(const char *state) {
    rapidjson::StringBuffer buf;
    RapidjsonWriter w(buf);
    w.StartObject();
    w.Key("state"); w.String(state);
    w.Key("total"); w.Int(g_mediaScanner.rescanTotal());
    w.Key("scanned"); w.Int(g_mediaScanner.rescanDone());
    w.Key("snapshot_version"); w.Int(g_mediaScanner.snapshotVersion());
    w.Key("version"); w.Int(g_mediaScanner.snapshotVersion());
    w.EndObject();
    return buf.GetString();
}

LocalServer *LocalServer::getInstance() {
    if (!_instance) {
        _instance = new LocalServer(g_profile.getString("LocalServer", "address", "127.0.0.1"),
                                    g_profile.getString("LocalServer", "http_port", "12120"),
                                    getLocal3WDir().c_str());
    }

    return _instance;
}

LocalServer::LocalServer(cstr_t address, cstr_t httpPort, cstr_t docRoot)
    : m_address(address)
    , m_httpPort(httpPort)
    , m_httpServer(address, httpPort) {
    HttpServer::EventStream::instance().bind(m_httpServer.ioContext());

    m_httpServer.registerRequestHandler(make_shared<HttpServer::ApiHandler>());

    auto handler = make_shared<HttpServer::StaticFilesHandler>("/", docRoot);
    m_httpServer.registerRequestHandler(handler);

    m_playerEventSender = make_shared<PlayerEventSender>();

    g_mediaScanner.setRescanListener([](const char *phase, int /*done*/, int /*total*/, int version) {
        auto &hub = HttpServer::EventStream::instance();
        if (strcmp(phase, "started") == 0) {
            hub.publish("library.scan_started", jsonScanPayload("running"));
        } else if (strcmp(phase, "progress") == 0) {
            hub.publish("library.scan_progress", jsonScanPayload("running"));
        } else if (strcmp(phase, "finished") == 0) {
            hub.publish("library.scan_finished", jsonScanPayload("finished"));
            rapidjson::StringBuffer buf;
            RapidjsonWriter w(buf);
            w.StartObject();
            w.Key("version"); w.Int(version);
            w.EndObject();
            hub.publish("library.updated", buf.GetString());
        }
    });
}

string LocalServer::getHttpBaseUrl() const {
    return "http://" + m_address + ":" + m_httpPort + "/";
}

void LocalServer::start() {
    assert(!m_threadHttpServer.isRunning());
    m_threadHttpServer.create(httpServerThread, this);
}

void LocalServer::httpServerThread(void *param) {
    LocalServer *server = (LocalServer *)param;

    try {
        server->m_httpServer.run();
    } catch (std::exception& e) {
        ERR_LOG1("Exception in httpServerThread: %s", e.what());
    }
}

void LocalServer::stop() {
}

