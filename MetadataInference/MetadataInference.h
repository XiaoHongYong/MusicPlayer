#pragma once

#include <string>
#include "MetadataResult.h"
#include "FieldInference.h"
#include "GarbageFilter.h"
#include "NumberParser.h"
#include "ConfidenceScorer.h"
#include "MPlayer/Media.h"
#include "Utils/Utils.h"

// 推断流程编排入口（docs/web-console/metadata.md §4/§5）。
namespace MetadataInference {

class InferenceEngine {
public:
    InferenceEngine();

    // 从文件名 + 路径 + 标签推断（纯逻辑，测试可直接调用）。
    MetadataResult infer(const InferInput &in) const;

    // 给一个 Media 做推断（内部组装 InferInput）。
    MetadataResult inferForMedia(const Media *media) const;

    // 把置信度达标的字段写进 Media（title/artist/album/year/track/disc/version + metaStatus）。
    // 返回是否发生变更。绝不改写音频文件。
    bool applyToMedia(const MetadataResult &r, Media *media) const;

    const GarbageFilter &garbage() const { return _garbage; }

private:
    GarbageFilter _garbage;
    NumberParser _numbers;
    FieldInference _field;
};

} // namespace MetadataInference

// 全局推断器（对其他模块公开）。
extern MetadataInference::InferenceEngine g_metadataInference;