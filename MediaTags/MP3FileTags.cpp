//
//  MP3FileTags.cpp
//  Taglib
//
//  Created by henry_xiao on 2023/9/11.
//

#include "MP3FileTags.hpp"
#include "ID3v2IF.h"
#include "ID3/ID3v1.h"
#include "MP3InfoReader.h"
#include "Lyrics3v2.h"
#include "LrcParser.h"


namespace MediaTags {

// 返回支持的媒体文件扩展名，不包含 '.'
VecStrings MP3FileTags::getSupportedExtNames() {
    return {"mp3", "mp2"};
}

VecStrings MP3FileTags::getSupportedSavingLyricsUrls() {
    return { SZ_SONG_ID3V2_SYLT, SZ_SONG_ID3V2_USLT, SZ_SONG_LYRICS3V2, SZ_SONG_ID3V2_LYRICS };
}

string MP3FileTags::getSuggestedSavingLyricsUrl() {
    return SZ_SONG_ID3V2_SYLT;
}

string MP3FileTags::getFileKind(const StringView &fileExt) {
    if (fileExt.iEqual("mp3")) {
        return "MPEG-1 Audio Layer 3 (mp3)";
    } else {
        return "MPEG-1 Audio Layer 2 (mp2)";
    }
}

int MP3FileTags::getTags(cstr_t fileName, BasicMediaTags &tagsOut, ExtendedMediaInfo &extendedInfoOut) {
    FilePtr fp;
    if (!fp.open(fileName, "rb")) {
        return ERR_OPEN_FILE;
    }

    BasicMediaTags vTags[2];
    CID3v1 id3v1;
    CID3v2IF id3v2(ED_SYSDEF);

    id3v1.getTag(fp.ptr(), vTags[1]);
    id3v2.open(fp.ptr(), false);
    id3v2.getTags(vTags[0]);

    mergeMediaTags(vTags, CountOf(vTags), tagsOut);

    MP3Info info;
    if (readMP3Info(fp.ptr(), info)) {
        extendedInfoOut.mediaLength = info.duration;
        extendedInfoOut.bitRate = info.bitRate;
        extendedInfoOut.channels = info.channelMode == MP3Info::SingleChannel ? 1 : 2;
        extendedInfoOut.bitsPerSample = info.bitsPerSample;
        extendedInfoOut.sampleRate = info.sampleRate;
    }

    return ERR_OK;
}

int MP3FileTags::setBasicTags(cstr_t fileName, const BasicMediaTags &tags) {
    FilePtr fp;
    if (!fp.open(fileName, "r+b")) {
        return ERR_OPEN_FILE;
    }

    CID3v2IF id3v2(ED_SYSDEF);

    int ret = id3v2.open(fp.ptr(), true);
    if (ret == ERR_OK) {
        id3v2.setTags(tags);
        ret = id3v2.save();
    }

    CID3v1 id3v1;
    if (id3v1.hasTag(fp.ptr())) {
        ret = id3v1.saveTag(fp.ptr(), tags);
    }

    return ret;
}

VecStrings MP3FileTags::enumLyrics(cstr_t fileName) {
    VecStrings lyricsUrls;

    FilePtr fp;
    if (!fp.open(fileName, "rb")) {
        return lyricsUrls;
    }

    CID3v2IF id3v2(ED_SYSDEF);
    int ret = id3v2.open(fp.ptr(), true);
    if (ret == ERR_OK) {
        id3v2.listLyrics(lyricsUrls);
    }

    // Put Sync lyrics at start
    VecStrings syncLyrics, unsyncLyrics;
    for (auto &str : lyricsUrls) {
        StringView url(str);
        if (url.startsWith(SZ_SONG_ID3V2_SYLT) || url.startsWith(SZ_SONG_ID3V2_LYRICS)) {
            syncLyrics.push_back(str);
        } else {
            unsyncLyrics.push_back(str);
        }
    }

    Lyrics3v2 lyr3v2;
    if (lyr3v2.exists(fp.ptr())) {
        syncLyrics.push_back(SZ_SONG_LYRICS3V2);
    }

    syncLyrics.insert(syncLyrics.end(), unsyncLyrics.begin(), unsyncLyrics.end());
    return syncLyrics;
}

int MP3FileTags::getLyrics(cstr_t fileName, cstr_t lyricsUrl, bool isUseSpecifiedEncoding, CharEncodingType encodingSpecified, RawLyrics &rawLyricsOut) {
    FilePtr fp;
    if (!fp.open(fileName, "rb")) {
        return ERR_OPEN_FILE;
    }

    StringView url(lyricsUrl);

    if (url.startsWith(SZ_SONG_ID3V2)) {
        CID3v2IF id3v2(ED_SYSDEF);
        int ret = id3v2.open(fp.ptr(), true);
        if (ret != ERR_OK) {
            return ret;
        }

        if (url.startsWith(SZ_SONG_ID3V2_USLT)) {
            ID3v2UnsynchLyrics lyrics;
            ret = id3v2.getUnsyncLyrics(lyrics, lyricsUrl);
            if (ret != ERR_OK) {
                return ret;
            }

            return parseLyricsString(lyrics.m_strLyrics, false, rawLyricsOut);
        } else if (url.startsWith(SZ_SONG_ID3V2_SYLT)) {
            ID3v2SynchLyrics lyrics;
            ret = id3v2.getSyncLyrics(lyrics, lyricsUrl);
            if (ret != ERR_OK) {
                return ret;
            }

            LyricsLine *line = nullptr;
            for (LrcSyllable &syllable : lyrics.m_vSynLyrics) {
                if (syllable.bNewLine) {
                    line = NULL;
                    continue;
                }
                if (!line || lyrics.m_bAllSyllableIsNewLine) {
                    rawLyricsOut.push_back(LyricsLine(syllable.nTime, -1));
                    line = &rawLyricsOut.lines().back();
                }

                line->appendPiece(syllable.nTime, -1, syllable.strText);
            }

            LyrTagParser tagParser(rawLyricsOut.properties());
            tagParser.parseLrcAllTags(lyrics.m_strContentDesc);

            return ERR_OK;
        } else if (url.startsWith(SZ_SONG_ID3V2_LYRICS)) {
            ID3v2TextUserDefined lyrics;
            id3v2.getUserDefLyrics(lyrics);
            return parseLyricsString(lyrics.m_strValue.c_str(), false, rawLyricsOut);
        } else {
            assert(0 && "Not supported");
            return ERR_NOT_SUPPORT_FILE_FORMAT;
        }
    } else if (url.equal(SZ_SONG_LYRICS3V2)) {
        Lyrics3v2 lyr3v2;
        if (lyr3v2.exists(fp.ptr())) {
            string lyrics;
            int ret = lyr3v2.readLyrcs(fp.ptr(), lyrics);
            if (ret != ERR_OK) {
                return ret;
            }

            return parseLyricsBinary(lyrics, isUseSpecifiedEncoding, encodingSpecified, rawLyricsOut);
        }
    } else {
        assert(0 && "Not supported");
        return ERR_NOT_SUPPORT_FILE_FORMAT;
    }

    return ERR_OK;
}

int MP3FileTags::setLyrics(cstr_t fileName, const RawLyrics &rawLyrics) {
    // 覆盖当前设置的歌词，如果没有歌词，则保存为 lyrics3v2 类型的歌词?
    VecStrings lyricsUrls = enumLyrics(fileName);
    if (lyricsUrls.empty()) {
        lyricsUrls.push_back(SZ_SONG_LYRICS3V2);
    }

    return setLyrics(fileName, lyricsUrls, rawLyrics);
}

int MP3FileTags::setLyrics(cstr_t fileName, const VecStrings &lyricsUrls, const RawLyrics &rawLyrics) {
    FilePtr fp;
    if (!fp.open(fileName, "r+b")) {
        return ERR_OPEN_FILE;
    }

    int countLyrics3v2 = 0;
    for (StringView url : lyricsUrls) {
        if (url.equal(SZ_SONG_LYRICS3V2)) {
            Lyrics3v2 lyr3v2;
            int ret = lyr3v2.writeLyrcs(fp.ptr(), rawLyrics);
            if (ret != ERR_OK) {
                return ret;
            }
            countLyrics3v2 = 1;
            break;
        }
    }

    if (lyricsUrls.size() > countLyrics3v2) {
        // 还有 id3v2 lyrics
        CID3v2IF id3v2(ED_SYSDEF);
        int ret = id3v2.open(fp.ptr(), true);
        if (ret != ERR_OK) {
            return ret;
        }

        for (auto &strUrl : lyricsUrls) {
            StringView url(strUrl);
            if (url.startsWith(SZ_SONG_ID3V2_USLT)) {
                auto content = toLyricsString(rawLyrics, true, true);
                id3v2.setUnsynchLyrics(strUrl.c_str(), "", content.c_str());
            } else if (url.startsWith(SZ_SONG_ID3V2_SYLT)) {
                if (rawLyrics.contentType() >= LCT_LRC) {
                    ID3v2SynchLyrics lyrics;

                    LyricsLines lyrLines = rawLyrics.toLyricsLinesOnly(0);
                    int offsetTime = rawLyrics.properties().getOffsetTime();

                    for (auto &line : lyrLines) {
                        for (auto &piece : line.pieces) {
                            auto t = piece.beginTime - offsetTime;
                            lyrics.addSynable(t < 0 ? 0 : t, piece.text);
                        }
                        lyrics.addNewLineSynable();
                    }

                    LyrTagParser tagParser(const_cast<LyricsProperties&>(rawLyrics.properties()));
                    lyrics.m_strContentDesc = tagParser.toLrcTags();
                    id3v2.setSynchLyrics(strUrl.c_str(), lyrics);
                }
            } else if (url.startsWith(SZ_SONG_ID3V2_LYRICS)) {
                string lyrics = toLyricsString(rawLyrics);
                id3v2.setUserDefLyrics(lyrics.c_str());
            } else if (url.equal(SZ_SONG_LYRICS3V2)) {
                // pass
            } else {
                assert(0 && "Not supported");
                return ERR_NOT_SUPPORT_FILE_FORMAT;
            }
        }

        return id3v2.save();
    }

    return ERR_OK;
}

int MP3FileTags::removeLyrics(cstr_t fileName, const VecStrings &lyrUrlsToRemove) {
    FilePtr fp;
    if (!fp.open(fileName, "r+b")) {
        return ERR_OPEN_FILE;
    }

    int countLyrics3v2 = 0;
    for (StringView url : lyrUrlsToRemove) {
        if (url.equal(SZ_SONG_LYRICS3V2)) {
            Lyrics3v2 lyr3v2;
            int ret = lyr3v2.clearLyricsInfo(fp);
            if (ret != ERR_OK) {
                return ret;
            }
        }
    }

    if (lyrUrlsToRemove.size() > countLyrics3v2) {
        // 还有 id3v2 lyrics
        CID3v2IF id3v2(ED_SYSDEF);
        int ret = id3v2.open(fp.ptr(), true);
        if (ret != ERR_OK) {
            return ret;
        }

        for (auto &strUrl : lyrUrlsToRemove) {
            StringView url(strUrl);
            if (url.startsWith(SZ_SONG_ID3V2_USLT) || url.startsWith(SZ_SONG_ID3V2_SYLT)) {
                id3v2.removeLyrics(strUrl.c_str());
            } else {
                assert(0 && "Not supported");
                return ERR_NOT_SUPPORT_FILE_FORMAT;
            }
        }

        return id3v2.save();
    }

    return ERR_OK;
}

int MP3FileTags::getPicture(cstr_t fileName, uint32_t index, string &imageDataOut) {
    FilePtr fp;
    if (!fp.open(fileName, "rb")) {
        return ERR_OPEN_FILE;
    }

    CID3v2IF id3v2(ED_SYSDEF);
    int ret = id3v2.open(fp.ptr(), true);
    if (ret != ERR_OK) {
        return ret;
    }

    ID3v2Pictures pictures;
    id3v2.getPictures(pictures);

    if (index >= pictures.m_vItems.size()) {
        return ERR_NOT_FOUND;
    }

    imageDataOut = pictures.m_vItems[index]->m_buffPic;
    return ERR_OK;
}

void MP3FileTags::getPictures(cstr_t fileName, VecStrings &vImagesDataOut) {
    FilePtr fp;
    if (!fp.open(fileName, "rb")) {
        return;
    }

    CID3v2IF id3v2(ED_SYSDEF);
    int ret = id3v2.open(fp.ptr(), true);
    if (ret != ERR_OK) {
        return;
    }

    ID3v2Pictures pictures;
    id3v2.getPictures(pictures);

    for (auto pic : pictures.m_vItems) {
        vImagesDataOut.push_back(pic->m_buffPic);
    }
}

int MP3FileTags::setPictures(cstr_t fileName, const VecStrings &picturesData) {
    FilePtr fp;
    if (!fp.open(fileName, "r+b")) {
        return ERR_OPEN_FILE;
    }

    CID3v2IF id3v2(ED_SYSDEF);
    int ret = id3v2.open(fp.ptr(), true);
    if (ret != ERR_OK) {
        return ret;
    }

    id3v2.setPictures(picturesData);

    return id3v2.save();
}

} // namespace MediaTags
