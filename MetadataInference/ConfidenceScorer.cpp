#include "ConfidenceScorer.h"
#include <algorithm>

namespace MetadataInference {

float ConfidenceScorer::overallConfidence(const MetadataResult &r) {
    float m = 0.0f;
    m = std::max(m, r.title.confidence);
    m = std::max(m, r.artist.confidence);
    m = std::max(m, r.album.confidence);
    return m;
}

bool ConfidenceScorer::shouldAutoApply(const MetadataResult &r) {
    return !r.isEmpty() && overallConfidence(r) >= 0.8f;
}

bool ConfidenceScorer::needsReview(const MetadataResult &r) {
    return !r.isEmpty() && !shouldAutoApply(r);
}

} // namespace MetadataInference