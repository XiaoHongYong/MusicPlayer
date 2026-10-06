//
//  EventStream.cpp
//  MusicPlayer
//

#include "EventStream.hpp"
#include <algorithm>
#include <chrono>

namespace HttpServer {

EventStream &EventStream::instance() {
    static EventStream s;
    return s;
}

void EventStream::bind(asio::io_context &io) {
    m_io = &io;
    m_timer = std::make_unique<asio::steady_timer>(io);
    scheduleHeartbeat();
}

void EventStream::addSubscriber(const ConnectionPtr &connection) {
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
}

void EventStream::removeSubscriber(const ConnectionPtr &connection) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_clients.erase(std::remove(m_clients.begin(), m_clients.end(), connection), m_clients.end());
}

int64_t EventStream::bumpStateVersion() {
    return ++m_stateVersion;
}

std::string EventStream::formatMessage(const char *event, const std::string &dataJson, int64_t id) const {
    std::string wire;
    wire += "id: ";
    wire += std::to_string(id);
    wire += "\nevent: ";
    wire += event;
    wire += "\ndata: {\"event\":\"";
    wire += event;
    wire += "\",\"state_version\":";
    wire += std::to_string(id);
    wire += ",\"data\":";
    wire += dataJson;
    wire += "}\n\n";
    return wire;
}

void EventStream::sendTo(const ConnectionPtr &connection, const char *event, const std::string &dataJson, int64_t id) {
    if (!connection) {
        return;
    }
    connection->writeStream(formatMessage(event, dataJson, id));
}

void EventStream::doPublish(const std::string &wire) {
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

void EventStream::publish(const char *event, const std::string &dataJson) {
    if (!m_io || !event) {
        return;
    }
    int64_t id = bumpStateVersion();
    std::string wire = formatMessage(event, dataJson, id);
    asio::post(*m_io, [this, wire]() {
        doPublish(wire);
    });
}

void EventStream::scheduleHeartbeat() {
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
