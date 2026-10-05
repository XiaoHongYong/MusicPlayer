#include "AlbumArtQuery.h"
#include "../MPlayer/Player.h"
#include "../Utils/App.h"


static void compactSpaces(string &text) {
    string out;
    out.reserve(text.size());
    bool prevSpace = false;
    for (size_t i = 0; i < text.size(); i++) {
        unsigned char c = (unsigned char)text[i];
        if (c == '\t' || c == '\r' || c == '\n') {
            c = ' ';
        }
        if (c == ' ') {
            if (prevSpace || out.empty()) {
                continue;
            }
            prevSpace = true;
            out += ' ';
            continue;
        }
        prevSpace = false;
        out += (char)c;
    }
    trimStr(out);
    text.swap(out);
}

string normalizeMediaIdentityField(cstr_t text) {
    string s = text ? text : "";
    compactSpaces(s);
    return toLower(s.c_str());
}

bool mediaIdentityIsEmpty(const MediaIdentity &id) {
    return id.artist.empty() && id.album.empty() && id.title.empty();
}

bool mediaIdentitiesEqual(const MediaIdentity &a, const MediaIdentity &b) {
    return strIsISame(a.artist.c_str(), b.artist.c_str())
        && strIsISame(a.album.c_str(), b.album.c_str())
        && strIsISame(a.title.c_str(), b.title.c_str());
}

string albumArtHistoryKey(const MediaIdentity &id) {
    string artist = normalizeMediaIdentityField(id.artist.c_str());
    string album = normalizeMediaIdentityField(id.album.c_str());
    string title = normalizeMediaIdentityField(id.title.c_str());
    if (!album.empty()) {
        return artist + "|" + album;
    }
    if (!artist.empty() || !title.empty()) {
        return artist + "|" + title;
    }
    return "";
}

string albumArtFileTitle(const MediaIdentity &id) {
    string name;
    if (!id.album.empty()) {
        if (!id.artist.empty()) {
            name = formatMediaTitle(id.artist.c_str(), id.album.c_str());
        } else {
            name = id.album;
        }
    } else {
        name = formatMediaTitle(id.artist.c_str(), id.title.c_str());
    }
    if (name.empty()) {
        name = "Folder";
    }
    return fileNameFilterInvalidChars(name.c_str());
}

string albumArtAppDataDir() {
    string dir = dirStringJoin(getAppDataDir().c_str(), "AlbumArt");
    dirStringAddSep(dir);
    return dir;
}

string albumArtAppDataPath(const MediaIdentity &id, cstr_t ext) {
    return albumArtAppDataDir() + albumArtFileTitle(id) + (ext ? ext : ".jpg");
}

static string folderNameOfFile(cstr_t filePath) {
    if (isEmptyString(filePath)) {
        return "";
    }
    string dir = fileGetPath(filePath);
    if (!dir.empty() && dir.back() == PATH_SEP_CHAR) {
        dir.pop_back();
    }
    if (dir.empty()) {
        return "";
    }
    return fileGetName(dir.c_str());
}

static string parentFolderNameOfFile(cstr_t filePath) {
    if (isEmptyString(filePath)) {
        return "";
    }
    string dir = fileGetPath(filePath);
    if (!dir.empty() && dir.back() == PATH_SEP_CHAR) {
        dir.pop_back();
    }
    if (dir.empty()) {
        return "";
    }
    return folderNameOfFile(dir.c_str());
}

static bool isGenericFolderName(const string &name) {
    static const char *kGeneric[] = {
        "music", "musics", "download", "downloads", "mp3", "flac",
        "itunes", "itunes media", "media", "songs", "audio",
    };
    string n = toLower(name.c_str());
    for (auto *g : kGeneric) {
        if (n == g) {
            return true;
        }
    }
    return n.empty() || n == "." || n == "..";
}

static void stripTrackPrefix(string &title) {
    size_t i = 0;
    while (i < title.size() && isDigit((unsigned char)title[i])) {
        i++;
    }
    if (i == 0 || i > 3) {
        return;
    }
    size_t j = i;
    while (j < title.size() && (title[j] == ' ' || title[j] == '.' || title[j] == '-' || title[j] == '_')) {
        j++;
    }
    if (j > i && j < title.size()) {
        title = title.substr(j);
        trimStr(title);
    }
}

static void addCandidate(VecMediaIdentities &out, MediaIdentity id) {
    trimStr(id.artist);
    trimStr(id.album);
    trimStr(id.title);
    if (id.album.empty() && (id.artist.empty() || id.title.empty())) {
        return;
    }
    for (auto &exist : out) {
        if (mediaIdentitiesEqual(exist, id)) {
            return;
        }
    }
    out.push_back(id);
}

VecMediaIdentities extractMediaIdentityCandidates(cstr_t artist, cstr_t album, cstr_t title, cstr_t filePath) {
    VecMediaIdentities out;

    string tagArtist = artist ? artist : "";
    string tagAlbum = album ? album : "";
    string tagTitle = title ? title : "";
    trimStr(tagArtist);
    trimStr(tagAlbum);
    trimStr(tagTitle);

    string fileArtist, fileTitle;
    if (!isEmptyString(filePath)) {
        getArtistTitleFromFileName(fileArtist, fileTitle, filePath);
        stripTrackPrefix(fileTitle);
    }

    string folderAlbum = folderNameOfFile(filePath);
    string folderArtist = parentFolderNameOfFile(filePath);
    if (isGenericFolderName(folderAlbum)) {
        folderAlbum.clear();
    }
    if (isGenericFolderName(folderArtist)) {
        folderArtist.clear();
    }

    // 1. 标签中的 artist + album（最准确）
    if (!tagArtist.empty() && !tagAlbum.empty()) {
        addCandidate(out, { tagArtist, tagAlbum, tagTitle });
    }

    // 2. 标签 artist + 文件夹名作 album
    if (!tagArtist.empty() && !folderAlbum.empty()
        && !strIsISame(folderAlbum.c_str(), tagAlbum.c_str())
        && !strIsISame(folderAlbum.c_str(), tagArtist.c_str())) {
        addCandidate(out, { tagArtist, folderAlbum, tagTitle.empty() ? fileTitle : tagTitle });
    }

    // 3. 文件夹 artist/album 结构
    if (!folderArtist.empty() && !folderAlbum.empty()
        && !strIsISame(folderArtist.c_str(), folderAlbum.c_str())) {
        string t = !tagTitle.empty() ? tagTitle : fileTitle;
        addCandidate(out, { folderArtist, folderAlbum, t });
    }

    // 4. 标签 artist + title（无专辑名时用曲名搜）
    if (!tagArtist.empty() && !tagTitle.empty()) {
        addCandidate(out, { tagArtist, "", tagTitle });
    }

    // 5. 文件名解析的 artist - title
    if (!fileArtist.empty() && !fileTitle.empty()) {
        string al = !tagAlbum.empty() ? tagAlbum : folderAlbum;
        addCandidate(out, { fileArtist, al, fileTitle });
        if (!al.empty()) {
            addCandidate(out, { fileArtist, "", fileTitle });
        }
    }

    // 6. 仅专辑名
    if (!tagAlbum.empty()) {
        addCandidate(out, { tagArtist, tagAlbum, tagTitle });
    }

    return out;
}

#if UNIT_TEST

#include "utils/unittest.h"

TEST(AlbumArtQuery, extractFromTags) {
    auto ids = extractMediaIdentityCandidates("Pink Floyd", "The Wall", "Hey You",
        "/Music/Pink Floyd/The Wall/03 - Hey You.mp3");
    ASSERT_FALSE(ids.empty());
    ASSERT_EQ(ids[0].artist, "Pink Floyd");
    ASSERT_EQ(ids[0].album, "The Wall");
    ASSERT_EQ(ids[0].title, "Hey You");
    ASSERT_EQ(albumArtHistoryKey(ids[0]), "pink floyd|the wall");
}

TEST(AlbumArtQuery, extractFromFileName) {
    auto ids = extractMediaIdentityCandidates("", "", "",
        "/tmp/Radiohead - Creep.mp3");
    ASSERT_FALSE(ids.empty());
    bool found = false;
    for (auto &id : ids) {
        if (id.artist == "Radiohead" && id.title == "Creep") {
            found = true;
        }
    }
    ASSERT_TRUE(found);
}

#endif
