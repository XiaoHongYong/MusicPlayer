#pragma once

#include "AlbumArtQuery.h"

// 在线封面搜索结果，按 score 从高到低使用。
struct AlbumArtSearchResult {
    int                         score = 0;
    string                      releaseId;
    string                      imageUrl;
    string                      sourceName;
};

using VecAlbumArtSearchResults = std::vector<AlbumArtSearchResult>;

class IAlbumArtSource {
public:
    virtual ~IAlbumArtSource() {}

    virtual cstr_t name() const = 0;
    virtual cstr_t lastError() const { return ""; }

    // 按准确度填充 results；失败返回 false。
    virtual bool search(const MediaIdentity &query, VecAlbumArtSearchResults &results) = 0;

    // 下载封面二进制；ext 形如 ".jpg"
    virtual bool downloadImage(const AlbumArtSearchResult &item, string &imageData, string &ext) = 0;
};

IAlbumArtSource *createDefaultAlbumArtSource();
// 国内兜底：网易云搜索 + iTunes 港台/美区
IAlbumArtSource *createChinaAlbumArtSource();
