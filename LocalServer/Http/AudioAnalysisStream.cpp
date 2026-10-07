#include "AudioAnalysisStream.hpp"
#include "../AudioAnalysisSender.hpp"
#include "MPlayer/Player.h"

#include <algorithm>
#include <chrono>

namespace HttpServer {

AudioAnalysisStream &AudioAnalysisStream::instance() {
    static AudioAnalysisStream s;
    return s;
}

void AudioAnalysisStream::bind(asio::io_context &io) {
    m_io = &io;
    m_timer = std::make_unique<asio::steady_timer>(io);
    scheduleHeartbeat();
}

int AudioAnalysisStream::subscriberCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return (int)m_clients.size();
}

void AudioAnalysisStream::syncPlayerSubscriptionLocked() {
    const bool want = !m_clients.empty();
    if (want == m_subscribedToPlayer) {
        return;
    }
    if (want) {
        AudioAnalysisOptions opt;
        opt.bandCount = 64;
        opt.outputRate = 30;
        opt.fftSize = 2048;
        opt.hopSize = 1024;
        g_player.setAudioAnalysisOptions(opt);
        g_player.subscribeAudioAnalysis(&AudioAnalysisSender::instance());
        m_subscribedToPlayer = true;
    } else {
        g_player.unsubscribeAudioAnalysis(&AudioAnalysisSender::instance());
        m_subscribedToPlayer = false;
    }
}

void AudioAnalysisStream::addSubscriber(const ConnectionPtr &connection) {
    if (!connection) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto &c : m_clients) {
        if (c == connection) {
            return;
        }
    }
    m_clients.push_back(connection);
    syncPlayerSubscriptionLocked();
}

void AudioAnalysisStream::removeSubscriber(const ConnectionPtr &connection) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_clients.erase(std::remove(m_clients.begin(), m_clients.end(), connection), m_clients.end());
    syncPlayerSubscriptionLocked();
}

std::string AudioAnalysisStream::formatMessage(const std::string &dataJson, int64_t id) const {
    std::string wire;
    wire += "id: ";
    wire += std::to_string(id);
    wire += "\nevent: audio_analysis\ndata: ";
    wire += dataJson;
    wire += "\n\n";
    return wire;
}

void AudioAnalysisStream::doPublish(const std::string &wire) {
    std::vector<ConnectionPtr> clients;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        clients = m_clients;
    }
    for (auto &c : clients) {
        if (c) {
            c->writeStream(wire);
        }
    }
}

void AudioAnalysisStream::publish(const std::string &dataJson) {
    if (!m_io) {
        return;
    }
    const int64_t id = ++m_seq;
    std::string wire = formatMessage(dataJson, id);
    asio::post(*m_io, [this, wire]() {
        doPublish(wire);
    });
}

void AudioAnalysisStream::scheduleHeartbeat() {
    if (!m_timer) {
        return;
    }
    m_timer->expires_after(std::chrono::seconds(15));
    m_timer->async_wait([this](const std::error_code &ec) {
        if (ec) {
            return;
        }
        doPublish(":\n\n");
        scheduleHeartbeat();
    });
}

} // namespace HttpServer
