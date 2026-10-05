#pragma once

#include "IAlbumArtSource.h"

class CMusicBrainzAlbumArtSource : public IAlbumArtSource {
public:
    CMusicBrainzAlbumArtSource();
    virtual ~CMusicBrainzAlbumArtSource();

    virtual cstr_t name() const override;
    virtual cstr_t lastError() const override { return m_lastError.c_str(); }
    virtual bool search(const MediaIdentity &query, VecAlbumArtSearchResults &results) override;
    virtual bool downloadImage(const AlbumArtSearchResult &item, string &imageData, string &ext) override;

    static string luceneEscape(const string &text);
    static string buildReleaseQuery(const MediaIdentity &query);
    static string buildRecordingQuery(const MediaIdentity &query);

protected:
    bool httpGet(cstr_t url, int &httpCode, string &body);
    bool searchReleases(const MediaIdentity &query, VecAlbumArtSearchResults &results);
    bool searchRecordings(const MediaIdentity &query, VecAlbumArtSearchResults &results);
    void waitForRateLimit();

protected:
    int64_t                     m_lastRequestMs = 0;
    string                      m_lastError;
};
