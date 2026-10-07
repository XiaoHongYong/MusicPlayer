#pragma once

#include "MetadataResult.h"

// 置信度评分与自动入库判定（docs/web-console/metadata.md §13）。
namespace MetadataInference {

class ConfidenceScorer {
public:
    // 综合姓名可用的 title/artist/album 置信度。
    static float overallConfidence(const MetadataResult &r);

    // 自动入库阈值：核心字段最高置信度 >= 0.8。
    static bool shouldAutoApply(const MetadataResult &r);

    // 是否需要人工审查（不能自动入库且已推断出某些字段）。
    static bool needsReview(const MetadataResult &r);
};

} // namespace MetadataInference