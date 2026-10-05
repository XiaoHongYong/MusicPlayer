#include "MediaTags.h"

#include "ID3/ID3v1.h"
#include "ID3/ID3v2.h"
#include "FlacFileTags.hpp"
#include "OggFileTags.hpp"
#include "MP3FileTags.hpp"
#include "M4aFileTags.hpp"
#include "AacFileTags.hpp"
#include "MidiFileTags.h"
#include "LrcParser.h"


using namespace MediaTags;

MapIMediaFileTags initMediaFileTags();

MapIMediaFileTags __mediaFileTags = initMediaFileTags();

void _addToMediaFileTagsMap(MapIMediaFileTags &m, const IMediaFileTagsPtr &mft) {
    auto exts = mft->getSupportedExtNames();
    for (auto &ext : exts) {
        assert(m.find(ext) == m.end());
        m[ext] = mft;
    }
}

MapIMediaFileTags initMediaFileTags() {
    MapIMediaFileTags m;

    _addToMediaFileTagsMap(m, make_shared<OggFileTags>());
    _addToMediaFileTagsMap(m, make_shared<MP3FileTags>());
    _addToMediaFileTagsMap(m, make_shared<M4aFileTags>());
    _addToMediaFileTagsMap(m, make_shared<AacFileTags>());
    _addToMediaFileTagsMap(m, make_shared<FlacFileTags>());
    _addToMediaFileTagsMap(m, make_shared<MidiFileTags>());

    return m;
}

IMediaFileTagsPtr getMediaFileTags(const char *fileName) {
    string ext = fileGetExt(fileName);
    if (ext.empty()) {
        return nullptr;
    }

    assert(ext[0] == '.');
    ext = toLower(ext.substr(1).c_str());

    auto it = __mediaFileTags.find(ext);
    if (it == __mediaFileTags.end()) {
        return nullptr;
    }

    return (*it).second;
}

#define MIN_EMBEDDED_LYR_SIZE    10        // Minum file size of embedded lyrics

IdToString    __id2strLyrSrcType[] = {
    { LST_ID3V2_SYLT, SZ_SONG_ID3V2_SYLT },
    { LST_ID3V2_USLT, SZ_SONG_ID3V2_USLT },
    { LST_LYRICS3V2, SZ_SONG_LYRICS3V2 },
    { LST_ID3V2_LYRICS, SZ_SONG_ID3V2_LYRICS },
    { LST_M4A_LYRICS, SZ_SONG_M4A_LYRICS },
    { LST_SOLE_EMBEDDED_LYRICS, SZ_SONG_SOLE_EMBEDDED_LYRICS },
    { LST_UNKNOWN, nullptr },
};

IdToString    __id2strLyrSrcTypeDesc[] = {
    { LST_ID3V2_SYLT, SZ_SONG_ID3V2_SYLT_DESC },
    { LST_ID3V2_USLT, SZ_SONG_ID3V2_USLT_DESC },
    { LST_LYRICS3V2, SZ_SONG_LYRICS3V2_DESC },
    { LST_ID3V2_LYRICS, SZ_SONG_ID3V2_LYRICS_DESC },
    { LST_M4A_LYRICS, SZ_SONG_M4A_LYRICS_DESC },
    { LST_SOLE_EMBEDDED_LYRICS, SZ_SONG_SOLE_EMBEDDED_LYRICS },
    { LST_NONE, SZ_SONG_NO_LYRICS_DESC },
    { LST_UNKNOWN, nullptr },
};

LRC_SOURCE_TYPE lyrSrcTypeFromName(cstr_t szLrcSource) {
    if (strcasecmp(szLrcSource, NONE_LYRCS) == 0) {
        return LST_NONE;
    }

    for (int i = 0; __id2strLyrSrcType[i].szId != nullptr; i++) {
        if (startsWith(szLrcSource, __id2strLyrSrcType[i].szId)) {
            return (LRC_SOURCE_TYPE)__id2strLyrSrcType[i].dwId;
        }
    }

    return LST_FILE;
}

cstr_t lyrSrcTypeToName(LRC_SOURCE_TYPE lst) {
    if (lst == LST_NONE) {
        return NONE_LYRCS;
    }
    return idToString(__id2strLyrSrcType, lst, "Unknown");
}

cstr_t lyrSrcTypeToDesc(LRC_SOURCE_TYPE lst) {
    return idToString(__id2strLyrSrcTypeDesc, lst, "Unknown");
}

bool getEmbeddedLyricsUrlInfo(cstr_t szName, string &language, int &index) {
    language.clear();
    index = 0;

    if (szName == nullptr) {
        return true; // No language or index is set.
    }

    szName = szName + strlen("song://");
    VecStrings vStr;

    strSplit(szName, '/', vStr);

    if (vStr.size() < 2) {
        return false;
    }

    //     type = vStr[0];
    //     subType = vStr[1];

    if (vStr.size() >= 3) {
        language = vStr[2];
    }

    if (vStr.size() >= 4) {
        index = atoi(vStr[3].c_str());
    }

    return true;
}


//////////////////////////////////////////////////////////////////////////

bool isStrInArray(cstr_t szArray[], cstr_t str) {
    for (int i = 0; szArray[i] != nullptr; i++) {
        if (strcasecmp(szArray[i], str) == 0) {
            return true;
        }
    }

    return false;
}

namespace MediaTags {

string getFileKind(cstr_t fileName) {
    auto ext = fileGetExt(fileName);
    if (!isEmptyString(ext)) {
        ext++;
    }

    auto mft = getMediaFileTags(fileName);
    if (mft) {
        return mft->getFileKind(ext);
    }

    return ext;
}

int getTags(cstr_t fileName, BasicMediaTags &tags, ExtendedMediaInfo &extendedInfo) {
    if (!isFileExist(fileName)) {
        setCustomErrorDesc(stringPrintf("File NOT exists: %s", fileName).c_str());
        return ERR_CUSTOM_ERROR;
    }

    auto mft = getMediaFileTags(fileName);
    if (mft) {
        int ret = mft->getTags(fileName, tags, extendedInfo);

        auto &genre = tags.genre;
        if (genre.size() > 2 && genre[0] == '(' && genre.back() == ')' && isDigit(genre[1])) {
            // Genre's format is: "(13)" , convert to "Pop"
            auto index = atoi(genre.c_str() + 1);
            auto text = CID3v1::getGenreDescription(index);
            if (!isEmptyString(text)) {
                tags.genre = text;
            }
        }

        return ret;
    }

    return ERR_NOT_SUPPORT_FILE_FORMAT;
}

bool canSaveBasicTags(cstr_t fileName) {
    auto mft = getMediaFileTags(fileName);
    if (mft) {
        return mft->canSaveBasicTags();
    }

    return false;
}

int setBasicTags(cstr_t fileName, BasicMediaTags &tags) {
    if (!isFileExist(fileName)) {
        setCustomErrorDesc(stringPrintf("File NOT exists: %s", fileName).c_str());
        return ERR_CUSTOM_ERROR;
    }

    auto mft = getMediaFileTags(fileName);
    if (mft) {
        return mft->setBasicTags(fileName, tags);
    }

    return ERR_NOT_SUPPORT_FILE_FORMAT;
}

bool isEmbeddedLyricsUrl(cstr_t lyricsName) {
    return StringView(lyricsName).startsWith("song://");
}

VecStrings getEmbeddedLyrics(cstr_t fileName) {
    VecStrings lyricsUrls;

    if (!isFileExist(fileName)) {
        return lyricsUrls;
    }

    auto mft = getMediaFileTags(fileName);
    if (mft) {
        return mft->enumLyrics(fileName);
    }

    return lyricsUrls;
}

bool canSaveEmbeddedLyrics(cstr_t fileName) {
    auto mft = getMediaFileTags(fileName);
    if (mft) {
        return !mft->getSupportedSavingLyricsUrls().empty();
    }

    return false;
}

VecStrings getSupportedEmbeddedLyricsUrls(cstr_t fileName) {
    auto mft = getMediaFileTags(fileName);
    if (mft) {
        return mft->getSupportedSavingLyricsUrls();
    }

    return {};
}

string getSuggestedEmbeddedLyricsUrl(cstr_t fileName) {
    auto mft = getMediaFileTags(fileName);
    if (mft) {
        return mft->getSuggestedSavingLyricsUrl();
    }

    return {};
}

int openEmbeddedLyrics(cstr_t fileName, cstr_t lyricsUrl, bool isUseSpecifiedEncoding, CharEncodingType encodingSpecified, RawLyrics &rawLyricsOut) {
    auto mft = getMediaFileTags(fileName);
    if (mft) {
        int ret = mft->getLyrics(fileName, lyricsUrl, isUseSpecifiedEncoding, encodingSpecified, rawLyricsOut);
        if (ret == ERR_OK && rawLyricsOut.properties().lyrContentType == LCT_UNKNOWN) {
            rawLyricsOut.analyzeContentType();
        }
        return ret;
    }

    return ERR_OK;
}

int saveEmbeddedLyrics(cstr_t fileName, const VecStrings &vLyricsUrls, const string &lyrics) {
    RawLyrics lyrLines;

    int ret = parseLyricsString(lyrics, false, lyrLines);
    if (ret != ERR_OK) {
        return ret;
    }

    return saveEmbeddedLyrics(fileName, vLyricsUrls, lyrLines);
}

int saveEmbeddedLyrics(cstr_t fileName, const VecStrings &lyricsUrls, const RawLyrics &rawLyrics) {
    auto mft = getMediaFileTags(fileName);
    if (!mft) {
        return ERR_NOT_SUPPORT_FILE_FORMAT;
    }

    return mft->setLyrics(fileName, lyricsUrls, rawLyrics);
}

// 保存一种或者多种嵌入到歌曲中的歌词
int saveEmbeddedLyrics(cstr_t fileName, const string &lyrics) {
    auto mft = getMediaFileTags(fileName);
    if (!mft) {
        return ERR_NOT_SUPPORT_FILE_FORMAT;
    }

    VecStrings lyricsUrls = mft->enumLyrics(fileName);

    if (lyricsUrls.empty()) {
        lyricsUrls = { mft->getSuggestedSavingLyricsUrl() };
    }

    return saveEmbeddedLyrics(fileName, lyricsUrls, lyrics);
}

int removeEmbeddedLyrics(cstr_t fileName, VecStrings &vLyrNamesToRemove, int *succeededCount) {
    auto mft = getMediaFileTags(fileName);
    if (mft) {
        return mft->removeLyrics(fileName, vLyrNamesToRemove);
    }

    return ERR_NOT_SUPPORT_FILE_FORMAT;
}

int getEmbeddedPicture(cstr_t fileName, int index, string &imageDataOut) {
    auto mft = getMediaFileTags(fileName);
    if (mft) {
        return mft->getPicture(fileName, index, imageDataOut);
    }

    return ERR_NOT_SUPPORT_FILE_FORMAT;
}

void getEmbeddedPictures(cstr_t fileName, VecStrings &imagesOut) {
    auto mft = getMediaFileTags(fileName);
    if (mft) {
        mft->getPictures(fileName, imagesOut);
    }
}

bool canSavePictures(cstr_t fileName) {
    auto mft = getMediaFileTags(fileName);
    if (mft) {
        return mft->canSavePictures();
    }

    return false;
}

int setEmbeddedPictures(cstr_t fileName, const VecStrings &pictures) {
    auto mft = getMediaFileTags(fileName);
    if (mft) {
        return mft->setPictures(fileName, pictures);
    }

    return ERR_NOT_SUPPORT_FILE_FORMAT;
}

} // namespace MediaTags


#if UNIT_TEST

#include "../TinyJS/utils/unittest.h"

bool operator == (const string &a, const StringView &b) {
    return b.equal(a);
}

static string getTestDataFolder() {
    string testDataFolder = fileGetPath(__FILE__);
    dirStringAddSep(testDataFolder);
    return testDataFolder;
}

void testReadTagsInDir(const string &path) {
    string fnTags = dirStringJoin(path.c_str(), "tags.txt");
    string tags;
    ASSERT_TRUE(readFile(fnTags.c_str(), tags));

    VecStrings lines;
    StringView(tags).split('\n', lines);

    bool b;
    BasicMediaTags tag;
    ExtendedMediaInfo extendedInfo;

    static StringView title("@title: ");
    static StringView artist("@artist: ");
    static StringView album("@album: ");
    static StringView year("@year: ");
    static StringView track("@track: ");
    static StringView genre("@genre: ");
    static StringView bitrate("@bitrate: ");
    static StringView sample_rate("@sample-rate: ");
    static StringView channels("@channels: ");
    static StringView length_ms("@length-ms: ");
    // static StringView comment("@comment: ");

    for (StringView line : lines) {
        if (line.startsWith("=== ")) {
            tag = BasicMediaTags();
            extendedInfo = ExtendedMediaInfo();

            auto fn = dirStringJoin(path.c_str(), line.substr(4, -1).toString().c_str());
            printf("Media file: %s\n", fn.c_str());

            int ret = MediaTags::getTags(fn.c_str(), tag, extendedInfo);
            ASSERT_EQ(ret, ERR_OK);
        } else if (line.startsWith(title)) { ASSERT_EQ(tag.title, line.substr(title.len, -1));
        } else if (line.startsWith(artist)) { ASSERT_EQ(tag.artist, line.substr(artist.len, -1));
        } else if (line.startsWith(album)) { ASSERT_EQ(tag.album, line.substr(album.len, -1));
        } else if (line.startsWith(year)) { ASSERT_EQ(tag.year, line.substr(year.len, -1));
        } else if (line.startsWith(track)) {
            auto trackNo = line.substr(track.len, -1);
            if (!(tag.trackNo.empty() && trackNo.equal("0"))) {
                ASSERT_EQ(tag.trackNo, trackNo);
            }
        } else if (line.startsWith(genre)) { ASSERT_EQ(tag.genre, line.substr(genre.len, -1));
        } else if (line.startsWith(length_ms)) {
            ASSERT_EQ(extendedInfo.mediaLength, line.substr(length_ms.len, -1).atoi(b));
        } else if (line.startsWith(bitrate)) {
            ASSERT_EQ(extendedInfo.bitRate, line.substr(bitrate.len, -1).atoi(b));
        } else if (line.startsWith(sample_rate)) {
            ASSERT_EQ(extendedInfo.sampleRate, line.substr(sample_rate.len, -1).atoi(b));
        } else if (line.startsWith(channels)) {
            ASSERT_EQ(extendedInfo.channels, line.substr(channels.len, -1).atoi(b));
        } else {
            if (line.equal("*** END ***")) {
                // 结束.
                return;
            }

            if (line.startsWith("@")) {
                printf("Line: %.*s\n", line.len, line.data);
            }
            ASSERT_FALSE(line.startsWith("@"));
        }
    }
}


TEST(MediaTagsTest, readTags) {
    string path = dirStringJoin(getTestDataFolder().c_str(), "samples");

    testReadTagsInDir(path);
    testReadTagsInDir(dirStringJoin(path.c_str(), "mp3", "vbr"));
}

void testWriteTagsInDir(const string &path) {

    FileFind finder;

    ASSERT_TRUE(finder.openDir(path.c_str()));

    VecStrings excludeFiles = {
        // These files are corruppted, can't save them.
        "id3v24_genre_null_byte.mp3",
        "id3v24-long-title.mp3",
        "id3image_without_description.mp3",
        "id3v22.TCO.genre.mp3",
        "id3_comment_utf_16_with_bom.mp3",
        "UTF16.mp3",
        "id3v1-latin1.mp3",
        "id3v1_does_not_overwrite_id3v2.mp3",
    };

    while (finder.findNext()) {
        string name = finder.getCurName();
        if (isInArray(name, excludeFiles)) {
            continue;
        }

        // name = "id3v24-long-title.mp3";
        auto fn = dirStringJoin(path.c_str(), name.c_str());
        if (!MediaTags::canSaveBasicTags(fn.c_str())) {
            continue;
        }

        printf("Media file: %s\n", fn.c_str());

        auto fnToSave = getUnittestTempDir() + fileGetName(fn.c_str());
        ASSERT_TRUE(copyFile(fn.c_str(), fnToSave.c_str(), false));

        BasicMediaTags tag;
        ExtendedMediaInfo extendedInfo;
        int ret = MediaTags::getTags(fnToSave.c_str(), tag, extendedInfo);
        ASSERT_EQ(ret, ERR_OK);

        tag.artist = "=artist=";
        tag.title = "=title=";
        tag.album = "=album=";
        tag.year = "=year=";
        tag.genre = "Ambient";
        tag.trackNo = "3,7";
        // tag.comments = "=comments=";

        ret = MediaTags::setBasicTags(fnToSave.c_str(), tag);
        ASSERT_EQ(ret, ERR_OK);

        BasicMediaTags tag2;
        ExtendedMediaInfo extendedInfo2;
        ret = MediaTags::getTags(fnToSave.c_str(), tag2, extendedInfo2);
        deleteFile(fnToSave.c_str());
        ASSERT_EQ(ret, ERR_OK);

        ASSERT_EQ(tag.artist, tag2.artist);
        ASSERT_EQ(tag.title, tag2.title);
        ASSERT_EQ(tag.album, tag2.album);
        ASSERT_EQ(tag.year, tag2.year);
        ASSERT_EQ(tag.genre, tag2.genre);
        ASSERT_EQ(tag.trackNo, tag2.trackNo);

        ASSERT_EQ(extendedInfo.mediaLength, extendedInfo2.mediaLength);
        ASSERT_EQ(extendedInfo.bitRate, extendedInfo2.bitRate);
        ASSERT_EQ(extendedInfo.channels, extendedInfo2.channels);
        ASSERT_EQ(extendedInfo.bitsPerSample, extendedInfo2.bitsPerSample);
        ASSERT_EQ(extendedInfo.sampleRate, extendedInfo2.sampleRate);
    }
}

TEST(MediaTagsTest, writeTags) {
    string path = dirStringJoin(getTestDataFolder().c_str(), "samples");

    testWriteTagsInDir(path);
    testWriteTagsInDir(dirStringJoin(path.c_str(), "mp3", "vbr"));
}

void testReadPicsInDir(const string &path) {
    FileFind finder;

    ASSERT_TRUE(finder.openDir(path.c_str()));

    string fnPic1 = dirStringJoin(path.c_str(), "test2.m4a.jpg");
    string fnPic2 = dirStringJoin(path.c_str(), "iso8859_with_image.m4a.jpg");
    string pic1, pic2;
    ASSERT_TRUE(readFile(fnPic1.c_str(), pic1));
    ASSERT_TRUE(readFile(fnPic2.c_str(), pic2));

    VecStrings imageFiles = {
        // These files are corruppted, can't save them.
        "id3_image_jfif.mp3",
        "cover_img.mp3",
        "test2.m4a",
        "12oz.mp3",
        "iso8859_with_image.m4a",
        "id3v22_image.mp3",
        "image-text-encoding.mp3",
    };

    for (auto &name : imageFiles) {
        auto fn = dirStringJoin(path.c_str(), name.c_str());

        printf("Media file: %s\n", fn.c_str());

        auto fnToSave = getUnittestTempDir() + fileGetName(fn.c_str());
        ASSERT_TRUE(copyFile(fn.c_str(), fnToSave.c_str(), false));

        BasicMediaTags tag;
        ExtendedMediaInfo extendedInfo;
        int ret = MediaTags::getTags(fnToSave.c_str(), tag, extendedInfo);
        ASSERT_EQ(ret, ERR_OK);

        VecStrings pics;
        MediaTags::getEmbeddedPictures(fnToSave.c_str(), pics);

        ASSERT_EQ(pics.size(), 1);

        auto fnPic = fn + ".jpg";
        string picData;
        ASSERT_TRUE(readFile(fnPic.c_str(), picData));
        ASSERT_EQ(pics[0], picData);

        if (MediaTags::canSavePictures(fnToSave.c_str())) {
            for (int i = 0; i < 2; i++) {
                VecStrings newPics;
                if (i == 0) {
                    // 第一轮添加/替换，第二轮为删除
                    newPics = { pic1, pic2 };
                }

                ret = MediaTags::setEmbeddedPictures(fnToSave.c_str(), newPics);
                ASSERT_EQ(ret, ERR_OK);

                VecStrings pics;
                MediaTags::getEmbeddedPictures(fnToSave.c_str(), pics);

                ASSERT_EQ(pics.size(), newPics.size());
                for (int k = 0; k < pics.size(); k++) {
                    ASSERT_EQ(pics[k], newPics[k]);
                }

                BasicMediaTags tag2;
                ExtendedMediaInfo extendedInfo2;
                ret = MediaTags::getTags(fnToSave.c_str(), tag2, extendedInfo2);
                ASSERT_EQ(ret, ERR_OK);

                ASSERT_EQ(tag.artist, tag2.artist);
                ASSERT_EQ(tag.title, tag2.title);
                ASSERT_EQ(tag.album, tag2.album);
                ASSERT_EQ(tag.year, tag2.year);
                ASSERT_EQ(tag.genre, tag2.genre);
                ASSERT_EQ(tag.trackNo, tag2.trackNo);

                ASSERT_EQ(extendedInfo.mediaLength, extendedInfo2.mediaLength);
                ASSERT_EQ(extendedInfo.bitRate, extendedInfo2.bitRate);
                ASSERT_EQ(extendedInfo.channels, extendedInfo2.channels);
                ASSERT_EQ(extendedInfo.bitsPerSample, extendedInfo2.bitsPerSample);
                ASSERT_EQ(extendedInfo.sampleRate, extendedInfo2.sampleRate);
            }

            deleteFile(fnToSave.c_str());
        }
    }
}

TEST(MediaTagsTest, readPictures) {
    string path = dirStringJoin(getTestDataFolder().c_str(), "samples");
    testReadPicsInDir(path);
}

void testReadWritePicsInDir(const string &path) {
    FileFind finder;

    ASSERT_TRUE(finder.openDir(path.c_str()));

    auto pathSamples = dirStringJoin(getTestDataFolder().c_str(), "samples");
    string fnPic1 = dirStringJoin(pathSamples.c_str(), "test2.m4a.jpg");
    string fnPic2 = dirStringJoin(pathSamples.c_str(), "iso8859_with_image.m4a.jpg");
    string pic1, pic2;
    ASSERT_TRUE(readFile(fnPic1.c_str(), pic1));
    ASSERT_TRUE(readFile(fnPic2.c_str(), pic2));

    while (finder.findNext()) {
        string name = finder.getCurName();

        // name = "lyr-chs.m4a";
        auto fn = dirStringJoin(path.c_str(), name.c_str());
        if (!MediaTags::canSavePictures(fn.c_str())) {
            continue;
        }

        printf("Media file: %s\n", fn.c_str());

        auto fnToSave = getUnittestTempDir() + fileGetName(fn.c_str());
        ASSERT_TRUE(copyFile(fn.c_str(), fnToSave.c_str(), false));

        BasicMediaTags tag;
        ExtendedMediaInfo extendedInfo;
        int ret = MediaTags::getTags(fnToSave.c_str(), tag, extendedInfo);
        ASSERT_EQ(ret, ERR_OK);

        if (MediaTags::canSavePictures(fnToSave.c_str())) {
            for (int i = 0; i < 2; i++) {
                VecStrings newPics;
                if (i == 0) {
                    // 第一轮添加/替换，第二轮为删除
                    newPics = { pic1, pic2 };
                }

                ret = MediaTags::setEmbeddedPictures(fnToSave.c_str(), newPics);
                ASSERT_EQ(ret, ERR_OK);

                VecStrings pics;
                MediaTags::getEmbeddedPictures(fnToSave.c_str(), pics);

                ASSERT_EQ(pics.size(), newPics.size());
                for (int k = 0; k < pics.size(); k++) {
                    ASSERT_EQ(pics[k], newPics[k]);
                }

                BasicMediaTags tag2;
                ExtendedMediaInfo extendedInfo2;
                ret = MediaTags::getTags(fnToSave.c_str(), tag2, extendedInfo2);
                ASSERT_EQ(ret, ERR_OK);

                ASSERT_EQ(tag.artist, tag2.artist);
                ASSERT_EQ(tag.title, tag2.title);
                ASSERT_EQ(tag.album, tag2.album);
                ASSERT_EQ(tag.year, tag2.year);
                ASSERT_EQ(tag.genre, tag2.genre);
                ASSERT_EQ(tag.trackNo, tag2.trackNo);

                ASSERT_EQ(extendedInfo.mediaLength, extendedInfo2.mediaLength);
                ASSERT_EQ(extendedInfo.bitRate, extendedInfo2.bitRate);
                ASSERT_EQ(extendedInfo.channels, extendedInfo2.channels);
                ASSERT_EQ(extendedInfo.bitsPerSample, extendedInfo2.bitsPerSample);
                ASSERT_EQ(extendedInfo.sampleRate, extendedInfo2.sampleRate);
            }

            deleteFile(fnToSave.c_str());
        }
    }
}

TEST(MediaTagsTest, getPictureNotFoundWhenNoCover) {
    string pic;
    string fnMp3 = dirStringJoin(getTestDataFolder().c_str(), "samples", "cbr.mp3");
    ASSERT_EQ(MediaTags::getEmbeddedPicture(fnMp3.c_str(), 0, pic), ERR_NOT_FOUND);

    string fnFlac = dirStringJoin(getTestDataFolder().c_str(), "samples", "no-tags.flac");
    ASSERT_EQ(MediaTags::getEmbeddedPicture(fnFlac.c_str(), 0, pic), ERR_NOT_FOUND);

    string fnCover = dirStringJoin(getTestDataFolder().c_str(), "samples", "cover_img.mp3");
    ASSERT_EQ(MediaTags::getEmbeddedPicture(fnCover.c_str(), 0, pic), ERR_OK);
    ASSERT_FALSE(pic.empty());
    ASSERT_EQ(MediaTags::getEmbeddedPicture(fnCover.c_str(), 1, pic), ERR_NOT_FOUND);
}

TEST(MediaTagsTest, readWritePictures) {
    string path = dirStringJoin(getTestDataFolder().c_str(), "test-pics");
    testReadWritePicsInDir(path);
}

#endif
