//
//  EventStream.hpp
//  MusicPlayer
//
//  SSE 推送：GET /api/v1/events，播放器 → 网页单向事件.
//

#ifndef Http_EventStream_hpp
#define Http_EventStream_hpp

#include <asio.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "Connection.hpp"

namespace HttpServer {

class EventStream {
public:
    static EventStream &instance();

    void bind(asio::io_context &io);

    void addSubscriber(const ConnectionPtr &connection);
    void removeSubscriber(const ConnectionPtr &connection);

    /** dataJson 必须是一个 JSON object 文本（不含外层 event 包装）. */
    void publish(const char *event, const std::string &dataJson);

    int64_t bumpStateVersion();
    int64_t stateVersion() const { return m_stateVersion.load(); }

    void sendTo(const ConnectionPtr &connection, const char *event, const std::string &dataJson, int64_t id);

private:
    EventStream() = default;

    void doPublish(const std::string &wire);
    void scheduleHeartbeat();
    std::string formatMessage(const char *event, const std::string &dataJson, int64_t id) const;

    asio::io_context                    *m_io = nullptr;
    std::unique_ptr<asio::steady_timer> m_timer;
    std::mutex                          m_mutex;
    std::vector<ConnectionPtr>          m_clients;
    std::atomic<int64_t>                m_stateVersion { 0 };
};

} // namespace HttpServer

#endif
