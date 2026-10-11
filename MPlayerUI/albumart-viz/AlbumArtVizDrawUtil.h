#pragma once

#include "AlbumArtVizColorUtil.h"
#include "../../ImageLib/RawImageData.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace albumart_viz {

inline RawImageDataPtr rotateCircularArt(const RawImageDataPtr &src, float angleRad) {
    if (!src || src->width <= 0 || src->height <= 0 || src->bitCount != 32) {
        return nullptr;
    }
    const int size = src->width;
    if (src->height != size) {
        return nullptr;
    }
    auto dst = createRawImageData(size, size, 32);
    if (!dst) {
        return nullptr;
    }
    rawImageBGRA32Set(dst.get(), 0, 0, 0, 0);
    const float cx = (size - 1) * 0.5f;
    const float cy = (size - 1) * 0.5f;
    const float radius2 = (size * 0.5f) * (size * 0.5f);
    const float c = std::cos(angleRad);
    const float s = std::sin(angleRad);
    for (int y = 0; y < size; ++y) {
        uint8_t *drow = dst->rowPtr(y);
        const float dy = y - cy;
        for (int x = 0; x < size; ++x) {
            const float dx = x - cx;
            if (dx * dx + dy * dy > radius2) {
                continue;
            }
            const float sx = c * dx + s * dy + cx;
            const float sy = -s * dx + c * dy + cy;
            const int ix = (int)std::lround(sx);
            const int iy = (int)std::lround(sy);
            if ((unsigned)ix >= (unsigned)size || (unsigned)iy >= (unsigned)size) {
                continue;
            }
            const uint8_t *sp = src->pixPtr(ix, iy);
            uint8_t *dp = drow + x * 4;
            dp[0] = sp[0];
            dp[1] = sp[1];
            dp[2] = sp[2];
            dp[3] = sp[3];
        }
    }
    return dst;
}

/** 绘制圆形封面；artSpinRad 非 0 时绕中心旋转。 */
inline void drawCircularAlbumArt(const AlbumArtVizContext &ctx, float cx, float cy, float artHalf,
                                 float paletteT = 0.4f, float artSpinRad = 0.f) {
    if (!ctx.canvas) {
        return;
    }
    CRawGraph *canvas = ctx.canvas;

    if (!ctx.albumArt || !ctx.albumArt->isValid() || ctx.albumArt->m_cx <= 0 ||
        ctx.albumArt->m_cy <= 0) {
        CColor disc = paletteColor(ctx, paletteT, 210);
        const int segs = 40;
        VecPoints poly;
        poly.reserve(segs);
        for (int i = 0; i < segs; ++i) {
            const float a = artSpinRad + (float)(2.0 * M_PI * i / segs);
            poly.push_back(CPoint((int)std::lround(cx + std::cos(a) * artHalf),
                                  (int)std::lround(cy + std::sin(a) * artHalf)));
        }
        canvas->fillPath(poly, disc);
        return;
    }

    const int ix = (int)std::lround(cx - artHalf);
    const int iy = (int)std::lround(cy - artHalf);
    const int isize = (int)std::lround(artHalf * 2.f);

    const CSFImage *drawImg = ctx.albumArt;
    CSFImage rotatedImg;
    if (std::fabs(artSpinRad) > 1e-4f) {
        auto rotated = rotateCircularArt(ctx.albumArt->getHandle(), artSpinRad);
        if (rotated) {
            rotatedImg.attach(rotated, ctx.scaleFactor);
            drawImg = &rotatedImg;
        }
    }

    CSFImage *img = const_cast<CSFImage *>(drawImg);
    if (img->m_cx == isize && img->m_cy == isize) {
        img->blt(canvas, ix, iy, BPM_BLEND);
    } else {
        img->stretchBlt(canvas, ix, iy, isize, isize, BPM_BLEND);
    }
}

inline void strokeCircle(CRawGraph *canvas, CRawPen &pen, float cx, float cy, float r, int segs,
                         float phase = 0.f) {
    if (!canvas || r <= 0.5f || segs < 8) {
        return;
    }
    canvas->setPen(pen);
    for (int i = 0; i < segs; ++i) {
        const float a0 = phase + (float)(2.0 * M_PI * i / segs);
        const float a1 = phase + (float)(2.0 * M_PI * (i + 1) / segs);
        canvas->line(cx + std::cos(a0) * r, cy + std::sin(a0) * r, cx + std::cos(a1) * r,
                     cy + std::sin(a1) * r);
    }
}

inline void fillSoftDisc(CRawGraph *canvas, float cx, float cy, float r, const CColor &clr,
                         int segs = 48) {
    if (!canvas || r <= 0.5f) {
        return;
    }
    VecPoints poly;
    poly.reserve(segs);
    for (int i = 0; i < segs; ++i) {
        const float a = (float)(2.0 * M_PI * i / segs);
        poly.push_back(CPoint((int)std::lround(cx + std::cos(a) * r),
                              (int)std::lround(cy + std::sin(a) * r)));
    }
    canvas->fillPath(poly, clr);
}

} // namespace albumart_viz
