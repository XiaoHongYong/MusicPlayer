#pragma once

#include <string>
#include "GarbageFilter.h"
#include "NumberParser.h"
#include "Utils/Utils.h"

// 文件名解析（docs/web-console/metadata.md，串起分词/垃圾过滤/序号/版本）。
namespace MetadataInference {

struct FilenameParseResult {
    bool hasSeparator = false;   // 是否有明确的 artist/title 分隔符
    std::string title;
    std::string artist;          // 候选（可能为空，由 FieldInference 决定用不用）
    std::string album;           // 候选（多段式 "Artist - Album - Title" 时的中间段）
    std::string version;
    int track = -1;
    int disc = -1;
    int year = 0;
};

class FilenameParser {
public:
    FilenameParser(const GarbageFilter &gf, const NumberParser &np) : _gf(gf), _np(np) {}

    // 传"无扩展名的文件名"（可含路径分隔，取 basename）。输出推断结果。
    FilenameParseResult parse(const std::string &fileNameNoExt) const;

private:
    const GarbageFilter &_gf;
    const NumberParser &_np;
};

} // namespace MetadataInference