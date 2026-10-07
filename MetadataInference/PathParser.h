#pragma once

#include <string>
#include "NumberParser.h"
#include "Utils/Utils.h"

// 路径层级解析（docs/web-console/metadata.md：从父目录结构出 Artist/Album/Year）。
namespace MetadataInference {

struct PathParseResult {
    std::string artist;   // 候选（命中第二个有意义目录）
    std::string album;    // 候选（最近的有意义目录）
    int year = 0;
    bool hasDirectory = false;
};

// 目录名是否为应跳过的分类/根层级（Genre 层），供 FieldInference 复用。
bool looksLikeGenreDir(const std::string &normalizedKey);

class PathParser {
public:
    PathParser(const NumberParser &np) : _np(np) {}

    // 传媒体文件完整路径。输出目录推断的候选值（source=Path）。
    PathParseResult parse(cstr_t filePath) const;

private:
    const NumberParser &_np;
};

} // namespace MetadataInference