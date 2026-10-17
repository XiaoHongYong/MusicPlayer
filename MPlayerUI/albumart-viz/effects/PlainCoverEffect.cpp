#include "PlainCoverEffect.h"
#include "../AlbumArtVizColorUtil.h"

#include <algorithm>
#include <cmath>

namespace {

void drawFittedAlbumArt(CRawGraph *canvas, const CSFImage *art, const CRect &bounds) {
    if (!canvas || !art || !art->isValid() || art->m_cx <= 0 || art->m_cy <= 0) {
        return;
    }
    const float dstW = (float)bounds.width();
    const float dstH = (float)bounds.height();
    if (dstW <= 0.f || dstH <= 0.f) {
        return;
    }

    const float scale = std::min(dstW / (float)art->m_cx, dstH / (float)art->m_cy);
    const int w = std::max(1, (int)std::lround((float)art->m_cx * scale));
    const int h = std::max(1, (int)std::lround((float)art->m_cy * scale));
    const int x = bounds.left + (bounds.width() - w) / 2;
    const int y = bounds.top + (bounds.height() - h) / 2;

    CSFImage *img = const_cast<CSFImage *>(art);
    img->stretchBlt(canvas, x, y, w, h, BPM_BLEND);
}

void fillPlaceholder(const AlbumArtVizContext &ctx) {
    if (!ctx.canvas) {
        return;
    }
    CColor fill = albumart_viz::paletteColor(ctx, 0.4f, 210);
    VecPoints poly;
    poly.push_back(CPoint(ctx.bounds.left, ctx.bounds.top));
    poly.push_back(CPoint(ctx.bounds.right, ctx.bounds.top));
    poly.push_back(CPoint(ctx.bounds.right, ctx.bounds.bottom));
    poly.push_back(CPoint(ctx.bounds.left, ctx.bounds.bottom));
    ctx.canvas->fillPath(poly, fill);
}

} // namespace

void PlainCoverEffect::draw(const AlbumArtVizContext &ctx) {
    if (!ctx.canvas) {
        return;
    }
    const CRect &rc = ctx.bounds;
    if (rc.width() <= 4 || rc.height() <= 4) {
        return;
    }

    const bool hasCur = ctx.albumArtOrg && ctx.albumArtOrg->isValid() && ctx.albumArtOrg->m_cx > 0 &&
                        ctx.albumArtOrg->m_cy > 0;
    const bool hasPrev = ctx.albumArtOrgPrev && ctx.albumArtOrgPrev->isValid() &&
                         ctx.albumArtOrgPrev->m_cx > 0 && ctx.albumArtOrgPrev->m_cy > 0;
    const float t = std::clamp(ctx.artCrossfade, 0.f, 1.f);
    const bool crossfading = hasPrev && t < 1.f;

    if (!hasCur && !hasPrev) {
        fillPlaceholder(ctx);
        return;
    }

    if (!crossfading) {
        if (hasCur) {
            drawFittedAlbumArt(ctx.canvas, ctx.albumArtOrg, rc);
        } else {
            fillPlaceholder(ctx);
        }
        return;
    }

    const int fromOpacity = (int)std::lround((1.f - t) * 255.f);
    const int toOpacity = (int)std::lround(t * 255.f);
    if (fromOpacity > 0) {
        CRawGraph::COpacityBlendAutoRecovery op(ctx.canvas, fromOpacity);
        drawFittedAlbumArt(ctx.canvas, ctx.albumArtOrgPrev, rc);
    }
    if (toOpacity > 0 && hasCur) {
        CRawGraph::COpacityBlendAutoRecovery op(ctx.canvas, toOpacity);
        drawFittedAlbumArt(ctx.canvas, ctx.albumArtOrg, rc);
    } else if (toOpacity > 0 && !hasCur) {
        CRawGraph::COpacityBlendAutoRecovery op(ctx.canvas, toOpacity);
        fillPlaceholder(ctx);
    }
}
