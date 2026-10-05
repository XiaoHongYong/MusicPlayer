//
//  AacFileTags.hpp
//

#ifndef AacFileTags_hpp
#define AacFileTags_hpp

#include "IMediaFileTags.hpp"


namespace MediaTags {

class AacFileTags : public IMediaFileTags {
public:
    // 返回支持的媒体文件扩展名，不包含 '.'
    VecStrings getSupportedExtNames() override { return { "aac" }; }
    bool canSaveBasicTags() override { return false; }

    string getFileKind(const StringView &fileExt) override;

    int getTags(cstr_t fileName, BasicMediaTags &tagsOut, ExtendedMediaInfo &extendedInfoOut) override;
    int setBasicTags(cstr_t fileName, const BasicMediaTags &tags) override { return ERR_NOT_SUPPORT_FILE_FORMAT; }

    VecStrings enumLyrics(cstr_t fileName) override;
    int getLyrics(cstr_t fileName, cstr_t lyricsUrl, bool isUseSpecifiedEncoding, CharEncodingType encodingSpecified, RawLyrics &rawLyricsOut) override;

    int getPicture(cstr_t fileName, uint32_t index, string &imageDataOut) override { return ERR_NOT_FOUND; }
    void getPictures(cstr_t fileName, VecStrings &vImagesDataOut) override { }

};

}

#endif /* AacFileTags_hpp */
