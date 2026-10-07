//
//  AudioAnalysisStream.hpp
//  独立 SSE：GET /api/v1/audio-analysis（高频分析帧，与 /events 分离）
//

#pragma once

#include <asio.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "Connection.hpp"

namespace HttpServer {

class AudioAnalysisStream {
public:
    static AudioAnalysisStream &instance();

    void bind(asio::io_context &io);

    void addSubscriber(const ConnectionPtr &connection);
    void removeSubscriber(const ConnectionPtr &connection);

    /** 推送分析 JSON object（不含 event 包装）. */
    void publish(const std::string &dataJson);

    int subscriberCount() const;

private:
    AudioAnalysisStream() = default;

    void doPublish(const std::string &wire);
    void scheduleHeartbeat();
    std::string formatMessage(const std::string &dataJson, int64_t id) const;
    void syncPlayerSubscriptionLocked();

    asio::io_context                    *m_io = nullptr;
    std::unique_ptr<asio::steady_timer> m_timer;
    mutable std::mutex                  m_mutex;
    std::vector<ConnectionPtr>          m_clients;
    std::atomic<int64_t>                m_seq { 0 };
    bool                                m_subscribedToPlayer = false;
};

} // namespace HttpServer
