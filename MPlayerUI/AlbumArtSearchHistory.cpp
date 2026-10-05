#include "MPlayerApp.h"
#include "AlbumArtSearchHistory.h"


static const int64_t kHistoryKeepSeconds = 30 * 24 * 60 * 60;

string CAlbumArtSearchHistory::historyFile() const {
    return getAppDataFile("AlbumArtSearchHistory.txt");
}

void CAlbumArtSearchHistory::pruneLocked() {
    int64_t now = (int64_t)getTimeInSecond();
    for (auto it = m_items.begin(); it != m_items.end(); ) {
        int64_t t = atoll(it->second.c_str());
        if (t <= 0 || now - t > kHistoryKeepSeconds) {
            it = m_items.erase(it);
        } else {
            ++it;
        }
    }
}

void CAlbumArtSearchHistory::load() {
    MutexAutolock lock(m_mutex);
    if (m_loaded) {
        return;
    }
    m_loaded = true;
    m_items.clear();

    string text;
    if (!readFile(historyFile().c_str(), text)) {
        return;
    }

    VecStrings lines;
    strSplit(text.c_str(), '\n', lines);
    for (auto &line : lines) {
        trimStr(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        string timeStr, key;
        if (!strSplit(line.c_str(), '\t', timeStr, key)) {
            continue;
        }
        trimStr(timeStr);
        trimStr(key);
        if (!key.empty() && !timeStr.empty()) {
            m_items[key] = timeStr;
        }
    }
    pruneLocked();
}

void CAlbumArtSearchHistory::save() {
    string text;
    {
        MutexAutolock lock(m_mutex);
        pruneLocked();
        text = "# AlbumArtSearchHistory\n";
        for (auto &kv : m_items) {
            text += kv.second;
            text += "\t";
            text += kv.first;
            text += "\n";
        }
    }
    writeFile(historyFile().c_str(), text.c_str(), text.size());
}

bool CAlbumArtSearchHistory::wasSearchedRecently(const string &key) const {
    if (key.empty()) {
        return false;
    }
    MutexAutolock lock(m_mutex);
    auto it = m_items.find(key);
    if (it == m_items.end()) {
        return false;
    }
    int64_t t = atoll(it->second.c_str());
    int64_t now = (int64_t)getTimeInSecond();
    return t > 0 && now - t <= kHistoryKeepSeconds;
}

bool CAlbumArtSearchHistory::wasSearchedRecently(const VecMediaIdentities &ids) const {
    MutexAutolock lock(m_mutex);
    int64_t now = (int64_t)getTimeInSecond();
    for (auto &id : ids) {
        string key = albumArtHistoryKey(id);
        if (key.empty()) {
            continue;
        }
        auto it = m_items.find(key);
        if (it == m_items.end()) {
            continue;
        }
        int64_t t = atoll(it->second.c_str());
        if (t > 0 && now - t <= kHistoryKeepSeconds) {
            return true;
        }
    }
    return false;
}

void CAlbumArtSearchHistory::recordSearch(const string &key) {
    if (key.empty()) {
        return;
    }
    {
        MutexAutolock lock(m_mutex);
        m_items[key] = std::to_string((long long)getTimeInSecond());
    }
    save();
}

void CAlbumArtSearchHistory::recordSearch(const VecMediaIdentities &ids) {
    bool any = false;
    {
        MutexAutolock lock(m_mutex);
        string now = std::to_string((long long)getTimeInSecond());
        for (auto &id : ids) {
            string key = albumArtHistoryKey(id);
            if (key.empty()) {
                continue;
            }
            m_items[key] = now;
            any = true;
        }
    }
    if (any) {
        save();
    }
}
