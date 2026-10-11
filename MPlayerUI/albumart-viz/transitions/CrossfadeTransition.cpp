#include "CrossfadeTransition.h"

#include <algorithm>

void CrossfadeTransition::begin(IAlbumArtEffect *from, IAlbumArtEffect *to, float durationSec) {
    m_from = from;
    m_to = to;
    m_durationSec = std::max(0.05f, durationSec);
    m_elapsedSec = 0.f;
    m_progress = 0.f;
    m_finished = (from == nullptr && to == nullptr);
}

bool CrossfadeTransition::update(float dtSec) {
    if (m_finished) {
        return false;
    }
    m_elapsedSec += std::max(0.f, dtSec);
    m_progress = std::clamp(m_elapsedSec / m_durationSec, 0.f, 1.f);
    if (m_progress >= 1.f) {
        m_finished = true;
        return false;
    }
    return true;
}

void CrossfadeTransition::cancel() {
    m_finished = true;
    m_progress = 1.f;
}

void CrossfadeTransition::draw(const AlbumArtVizContext &ctx) {
    if (!ctx.canvas) {
        return;
    }

    const float t = m_progress;
    const int fromOpacity = (int)std::lround((1.f - t) * 255.f);
    const int toOpacity = (int)std::lround(t * 255.f);

    if (m_from && fromOpacity > 0) {
        CRawGraph::COpacityBlendAutoRecovery op(ctx.canvas, fromOpacity);
        m_from->update(ctx, 0.f);
        m_from->draw(ctx);
    }
    if (m_to && toOpacity > 0) {
        CRawGraph::COpacityBlendAutoRecovery op(ctx.canvas, toOpacity);
        m_to->update(ctx, 0.f);
        m_to->draw(ctx);
    }
}
