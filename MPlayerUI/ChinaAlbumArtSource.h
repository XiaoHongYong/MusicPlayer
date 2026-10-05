#pragma once

#include "IAlbumArtSource.h"

// 国内封面源：先网易云 cloudsearch，失败再用 iTunes 港台/美区（无需 API Key）。
class CChinaAlbumArtSource : public IAlbumArtSource {
public:
    CChinaAlbumArtSource();
    virtual ~CChinaAlbumArtSource();

    virtual cstr_t name() const override;
    virtual cstr_t lastError() const override { return m_lastError.c_str(); }
    virtual bool search(const MediaIdentity &query, VecAlbumArtSearchResults &results) override;
    virtual bool downloadImage(const AlbumArtSearchResult &item, string &imageData, string &ext) override;

    static string buildKeyword(const MediaIdentity &query);

protected:
    bool httpGet(cstr_t url, int &httpCode, string &body, cstr_t referer);
    bool parseNeteaseSearch(const string &body, const MediaIdentity &query, int type, VecAlbumArtSearchResults &results);
    bool searchNetease(const MediaIdentity &query, int type, VecAlbumArtSearchResults &results);
    bool searchITunes(const MediaIdentity &query, const string &entity, VecAlbumArtSearchResults &results);
    void waitForRateLimit();

protected:
    int64_t                     m_lastRequestMs = 0;
    string                      m_lastError;
};
