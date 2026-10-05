//
//  IMediaFileTags.hpp
//  Taglib
//
//  Created by henry_xiao on 2023/9/6.
//

#ifndef IMediaFileTag_hpp
#define IMediaFileTag_hpp

#include "LyricsData.h"
#include "MediaTagTypes.hpp"


namespace MediaTags {

class IMediaFileTags {
public:
    virtual ~IMediaFileTags() { }

    // 返回支持的媒体文件扩展名，不包含 '.'
    virtual VecStrings getSupportedExtNames() = 0;

    virtual string getFileKind(const StringView &fileExt) = 0;

    virtual bool canSaveBasicTags() { return true; }
    virtual bool canSavePictures() { return false; }
    virtual VecStrings getSupportedSavingLyricsUrls() { return VecStrings(); }
    virtual string getSuggestedSavingLyricsUrl() {
        auto urls = getSupportedSavingLyricsUrls();
        return urls.empty() ? "" : urls[0];
    }

    virtual int getTags(cstr_t fileName, BasicMediaTags &tagsOut, ExtendedMediaInfo &extendedInfoOut) = 0;
    virtual int setBasicTags(cstr_t fileName, const BasicMediaTags &tags) { return ERR_NOT_SUPPORT_FILE_FORMAT; }

    virtual VecStrings enumLyrics(cstr_t fileName) { return VecStrings(); }
    virtual int getLyrics(cstr_t fileName, cstr_t lyricsUrl, bool isUseSpecifiedEncoding, CharEncodingType encodingSpecified, RawLyrics &rawLyricsOut) { return ERR_NOT_FOUND; }
    virtual int setLyrics(cstr_t fileName, const RawLyrics &rawLyrics) { return ERR_NOT_SUPPORT_FILE_FORMAT; }
    virtual int setLyrics(cstr_t fileName, const VecStrings &lyricsUrls, const RawLyrics &rawLyrics) { return ERR_NOT_SUPPORT_FILE_FORMAT; }
    virtual int removeLyrics(cstr_t fileName, const VecStrings &lyrUrlsToRemove) { return ERR_NOT_FOUND; }

    virtual int getPicture(cstr_t fileName, uint32_t index, string &imageDataOut) { return ERR_NOT_FOUND; }
    virtual void getPictures(cstr_t fileName, VecStrings &vImagesDataOut) { }
    virtual int setPictures(cstr_t fileName, const VecStrings &picturesData) { return ERR_NOT_SUPPORT_FILE_FORMAT; }

};

using IMediaFileTagsPtr = std::shared_ptr<IMediaFileTags>;
using MapIMediaFileTags = std::map<std::string, IMediaFileTagsPtr>;

}

#endif /* IMediaFileTag_hpp */
