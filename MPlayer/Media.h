#pragma once

#include <vector>
#include <string>
#include "Utils/Utils.h"
#include "MPlayerEngine/IPlayerCore.hpp"


using namespace std;

#define MEDIA_LENGTH_INVALID    0
#define MEDIA_ID_INVALID        0

class Media : public IMediaInfo {
public:
    Media();
    virtual ~Media();

    int                         ID = MEDIA_ID_INVALID;
    string                      url;
    string                      artist;
    string                      album;
    string                      title;
    int16_t                     trackNumb = -1;
    int16_t                     year = -1;
    string                      genre;
    string                      comments;

    int                         duration = MEDIA_LENGTH_INVALID;
    uint32_t                    fileSize = 0;
    time_t                      timeAdded = 0;
    time_t                      timePlayed = 0;

    uint16_t                    rating = 0;          // 0 ~ 500
    bool                        isUserRating = false;
    bool                        isFileDeleted = false;
    int                         countPlayed = 0;
    int                         countPlaySkipped = 0;

    string                      lyricsFile;
    string                      musicHash;

    string                      format;
    int                         bitRate = 0;
    bool                        isVbr = false;
    uint8_t                     channels = 0;
    uint8_t                     BPS = 0;
    bool                        infoUpdated = true;     // if InfoUpdated, it will be update to database when playing finished.
    uint32_t                    sampleRate = 0;
    string                      extraInfo;

    //
    // attribute methods
    //
    virtual void setAttribute(MediaAttribute mediaAttr, const char *value) override;
    virtual void setAttribute(MediaAttribute mediaAttr, int64_t value) override;

    void getAttribute(MediaAttribute mediaAttr, string &strValue);

};

using MediaPtr = std::shared_ptr<Media>;
using VecMediaPtrs = std::vector<MediaPtr>;
