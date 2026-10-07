#pragma once

#include <string>
#include "MetadataResult.h"
#include "FilenameParser.h"
#include "PathParser.h"
#include "NumberParser.h"
#include "GarbageFilter.h"

// 字段推断与合并（docs/web-console/metadata.md §11/§12/§13）：
// 综合 文件名 + 路径 候选，再按"可靠标签 > 高置信路径 > 文件名 > 弱启发"合并。
namespace MetadataInference {

struct InferInput {
    std::string filePath;         // 媒体完整路径（用于目录推断）
    std::string fileNameNoExt;    // 无扩展名的文件名
    std::string tagArtist, tagTitle, tagAlbum, tagGenre;
    int tagYear = 0;
    int tagTrack = -1;
};

class FieldInference {
public:
    FieldInference(const GarbageFilter &gf, const NumberParser &np)
        : _gf(gf), _np(np), _filename(gf, np), _path(np) {}

    MetadataResult infer(const InferInput &in) const;

private:
    float scoreForMetaField(const std::string &value, float base) const;

    const GarbageFilter &_gf;
    const NumberParser &_np;
    FilenameParser _filename;
    PathParser _path;
};

} // namespace MetadataInference