#pragma once

#include "Utils/Utils.h"
#include "MPlayerEngine/IPlayerCore.hpp"
#include "MetadataInference/MetadataResult.h"


#define MEDIA_LENGTH_INVALID    0
#define MEDIA_ID_INVALID        -1

class Media : public IMediaInfo {
public:
    Media();
    virtual ~Media();

    int                         ID = MEDIA_ID_INVALID;
    string                      url;
    string                      artist;
    string                      album;
    string                      title;
    string                      version;             // 标题版本词（Live/Remix/...），见 docs/web-console/metadata.md §9
    int16_t                     trackNumb = -1;
    int16_t                     discNumb = -1;
    int16_t                     year = 0;
    MetadataStatus              metaStatus = MetadataStatus::RAW;
    string                      genre;
    string                      comments;

    int                         duration = MEDIA_LENGTH_INVALID;
    uint64_t                    fileSize = 0;
    time_t                      timeAdded = 0;
    time_t                      timePlayed = 0;

    uint16_t                    rating = 0;          // 0 ~ 500
    bool                        isFileDeleted = false;
    int                         countPlayed = 0;

    string                      lyricsFile;
    string                      musicHash;

    string                      format;
    int                         bitRate = 0;
    uint8_t                     channels = 0;
    uint8_t                     bitsPerSample = 0;
    uint32_t                    sampleRate = 0;

    bool isEqual(const Media *other) const;

    //
    // attribute methods
    //
    virtual void setAttribute(MediaAttribute mediaAttr, const char *value) override;
    virtual void setAttribute(MediaAttribute mediaAttr, int64_t value) override;

    void getAttribute(MediaAttribute mediaAttr, string &strValue);

};

using MediaPtr = std::shared_ptr<Media>;
using VecMediaPtrs = std::vector<MediaPtr>;
