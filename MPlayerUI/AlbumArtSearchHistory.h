#pragma once

#include "AlbumArtQuery.h"

// 记录最近一个月内已搜索过的封面查询，避免重复请求。
class CAlbumArtSearchHistory {
public:
    void load();
    void save();

    bool wasSearchedRecently(const string &key) const;
    bool wasSearchedRecently(const VecMediaIdentities &ids) const;

    void recordSearch(const string &key);
    void recordSearch(const VecMediaIdentities &ids);

protected:
    void pruneLocked();
    string historyFile() const;

protected:
    mutable std::mutex          m_mutex;
    MapStrings                  m_items; // key -> unix seconds
    bool                        m_loaded = false;
};
