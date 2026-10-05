#pragma once

#include "../Utils/Utils.h"

// 一首歌可能对应多组 (artist, album, title)，按准确度从高到低排列。
struct MediaIdentity {
    string                      artist;
    string                      album;
    string                      title;
};

using VecMediaIdentities = std::vector<MediaIdentity>;

VecMediaIdentities extractMediaIdentityCandidates(cstr_t artist, cstr_t album, cstr_t title, cstr_t filePath);

string normalizeMediaIdentityField(cstr_t text);
string albumArtHistoryKey(const MediaIdentity &id);
string albumArtFileTitle(const MediaIdentity &id);
string albumArtAppDataDir();
string albumArtAppDataPath(const MediaIdentity &id, cstr_t ext);

bool mediaIdentityIsEmpty(const MediaIdentity &id);
bool mediaIdentitiesEqual(const MediaIdentity &a, const MediaIdentity &b);
