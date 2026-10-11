#include "SonicHaloEffect.h"
#include "../AlbumArtVizColorUtil.h"
#include "../../../ImageLib/RawImageData.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

constexpr float kPeakFall = 0.018f;
constexpr float kSpinSpeed = 0.004f;

RawImageDataPtr rotateCircularArt(const RawImageDataPtr &src, float angleRad) {
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

} // namespace

SonicHaloEffect::RotateMode SonicHaloEffect::rotateModeFromString(cstr_t s) {
    if (!s) {
        return RotateMode::SyncAll;
    }
    if (strcasecmp(s, "none") == 0 || strcasecmp(s, "off") == 0) {
        return RotateMode::None;
    }
    if (strcasecmp(s, "bars-only") == 0) {
        return RotateMode::BarsOnly;
    }
    return RotateMode::BarsOnly;
}

cstr_t SonicHaloEffect::rotateModeToString(RotateMode mode) {
    switch (mode) {
    case RotateMode::SyncAll:
        return "sync";
    case RotateMode::None:
        return "none";
    case RotateMode::BarsOnly:
    default:
        return "bars";
    }
}

bool SonicHaloEffect::setProperty(cstr_t name, cstr_t value) {
    if (strcasecmp(name, "RotateMode") == 0) {
        m_rotateMode = rotateModeFromString(value);
        return true;
    }
    return false;
}

float SonicHaloEffect::spinAngle(const AlbumArtVizContext &ctx) const {
    if (!ctx.hasFrame || !ctx.frame || m_rotateMode == RotateMode::None) {
        return 0.f;
    }
    return (float)ctx.frame->sequence * kSpinSpeed;
}

void SonicHaloEffect::updatePeakLevels(const AlbumArtVizContext &ctx) {
    const int bandCount = std::max(0, ctx.bandCount);
    if ((int)m_peakLevels.size() != bandCount) {
        m_peakLevels.assign(bandCount, 0.f);
    }
    if (!ctx.displayLevels || bandCount <= 0) {
        return;
    }
    for (int i = 0; i < bandCount; ++i) {
        float &peak = m_peakLevels[i];
        const float disp = ctx.displayLevels[i];
        if (disp > peak) {
            peak = disp;
        } else {
            peak = std::max(0.f, peak - kPeakFall);
        }
    }
}

void SonicHaloEffect::update(const AlbumArtVizContext &ctx, float /*dtSec*/) {
    updatePeakLevels(ctx);
}

void SonicHaloEffect::drawHaloRings(const AlbumArtVizContext &ctx, float cx, float cy, float artHalf,
                                    float maxR, float spin) {
    if (!ctx.canvas) {
        return;
    }
    CRawGraph *canvas = ctx.canvas;
    const float rms = (ctx.hasFrame && ctx.frame) ? ctx.frame->rms : 0.f;
    // 环厚度：lowEnergy；踢鼓：bassPulse；通用闪光：beatPulse
    const float lowE = ctx.lowEnergy;
    const float kick = ctx.bassPulse;
    const float flash = ctx.beatPulse;

    const int ringSeg = 64;
    const float glowR =
        artHalf + (maxR - artHalf) * (0.45f + ctx.ringSmooth * 0.35f + kick * 0.12f);
    CColor glow = albumart_viz::paletteColor(ctx, 0.55f + lowE * 0.3f,
                                             (uint8_t)(32 + flash * 45 + kick * 35));
    m_pen.createSolidPen(
        std::max(2, (int)std::lround(2.5f + lowE * 5.f + kick * 4.f + flash * 2.f)), glow);
    canvas->setPen(m_pen);
    for (int i = 0; i < ringSeg; ++i) {
        const float a0 = spin + (float)(2.0 * M_PI * i / ringSeg);
        const float a1 = spin + (float)(2.0 * M_PI * (i + 1) / ringSeg);
        const float wobbleAmt = ctx.ringSmooth * 0.03f + kick * 0.025f;
        const float wobble0 = 1.f + wobbleAmt * std::sin(a0 * 3.f + spin * 4.f);
        const float wobble1 = 1.f + wobbleAmt * std::sin(a1 * 3.f + spin * 4.f);
        canvas->line(cx + std::cos(a0) * glowR * wobble0, cy + std::sin(a0) * glowR * wobble0,
                     cx + std::cos(a1) * glowR * wobble1, cy + std::sin(a1) * glowR * wobble1);
    }

    CColor frameClr =
        albumart_viz::paletteColor(ctx, 0.7f, (uint8_t)(100 + flash * 90 + ctx.onsetStrength * 30));
    m_pen.createSolidPen(std::max(1, (int)std::lround(1.5f + flash * 2.5f)), frameClr);
    canvas->setPen(m_pen);
    const float rimR = artHalf * (0.98f + ctx.ringSmooth * 0.03f + kick * 0.02f);
    for (int i = 0; i < ringSeg; ++i) {
        const float a0 = (float)(2.0 * M_PI * i / ringSeg);
        const float a1 = (float)(2.0 * M_PI * (i + 1) / ringSeg);
        canvas->line(cx + std::cos(a0) * rimR, cy + std::sin(a0) * rimR,
                     cx + std::cos(a1) * rimR, cy + std::sin(a1) * rimR);
    }

    // 轨道弧：节奏置信度高时略加快相位感（仍用 sequence 自旋，仅抬亮度）
    const float orbitBoost = rms * 40.f + ctx.rhythmConfidence * 25.f + ctx.onsetStrength * 20.f;
    CColor orbit = albumart_viz::paletteColor(ctx, 0.8f, (uint8_t)(60 + orbitBoost));
    m_pen.createSolidPen(2, orbit);
    canvas->setPen(m_pen);
    const float rhythmSpin =
        spin * (1.f + std::clamp(ctx.bassRhythmRate, 0.f, 4.f) * 0.08f * ctx.rhythmConfidence);
    const float orbitR = artHalf + (maxR - artHalf) * 0.82f;
    for (int arc = 0; arc < 3; ++arc) {
        const float base =
            rhythmSpin * (1.2f + arc * 0.15f) + arc * (float)(2.0 * M_PI / 3.0);
        const int segs = 14;
        for (int i = 0; i < segs; ++i) {
            const float a0 = base + (float)i / segs * 0.55f;
            const float a1 = base + (float)(i + 1) / segs * 0.55f;
            canvas->line(cx + std::cos(a0) * orbitR, cy + std::sin(a0) * orbitR,
                         cx + std::cos(a1) * orbitR, cy + std::sin(a1) * orbitR);
        }
    }
}

void SonicHaloEffect::drawSpectrumRing(const AlbumArtVizContext &ctx, float cx, float cy,
                                       float baseR, float maxLen, float spin) {
    if (!ctx.canvas || !ctx.displayLevels || ctx.bandCount <= 0) {
        return;
    }
    CRawGraph *canvas = ctx.canvas;
    const int bandCount = ctx.bandCount;
    if ((int)m_peakLevels.size() != bandCount) {
        m_peakLevels.assign(bandCount, 0.f);
    }

    const float barWidth = (float)(2.0 * M_PI) / bandCount * 0.62f;

    for (int i = 0; i < bandCount; ++i) {
        const float aMid =
            spin + (float)i / (float)bandCount * (float)(2.0 * M_PI) - (float)(M_PI * 0.5);
        const float a0 = aMid - barWidth * 0.5f;
        const float a1 = aMid + barWidth * 0.5f;

        const float level = ctx.displayLevels[i];
        const float peak = m_peakLevels[i];
        const float len = std::max(1.5f, level * maxLen);
        const float peakLen = std::max(len, peak * maxLen);

        CColor bar = albumart_viz::rayColor(ctx, i, bandCount, (uint8_t)(170 + level * 70));
        VecPoints quad;
        quad.push_back(CPoint((int)std::lround(cx + std::cos(a0) * baseR),
                              (int)std::lround(cy + std::sin(a0) * baseR)));
        quad.push_back(CPoint((int)std::lround(cx + std::cos(a0) * (baseR + len)),
                              (int)std::lround(cy + std::sin(a0) * (baseR + len))));
        quad.push_back(CPoint((int)std::lround(cx + std::cos(a1) * (baseR + len)),
                              (int)std::lround(cy + std::sin(a1) * (baseR + len))));
        quad.push_back(CPoint((int)std::lround(cx + std::cos(a1) * baseR),
                              (int)std::lround(cy + std::sin(a1) * baseR)));
        canvas->fillPath(quad, bar);

        if (peak > 0.05f && peakLen > len + 1.f) {
            CColor tip = albumart_viz::rayColor(ctx, i, bandCount, 220);
            tip = albumart_viz::boostForViz(tip, 0.1f);
            tip.setAlpha(220);
            const float tipInner = baseR + peakLen - std::max(2.f, maxLen * 0.04f);
            const float tipOuter = baseR + peakLen;
            VecPoints tipQuad;
            tipQuad.push_back(CPoint((int)std::lround(cx + std::cos(a0) * tipInner),
                                     (int)std::lround(cy + std::sin(a0) * tipInner)));
            tipQuad.push_back(CPoint((int)std::lround(cx + std::cos(a0) * tipOuter),
                                     (int)std::lround(cy + std::sin(a0) * tipOuter)));
            tipQuad.push_back(CPoint((int)std::lround(cx + std::cos(a1) * tipOuter),
                                     (int)std::lround(cy + std::sin(a1) * tipOuter)));
            tipQuad.push_back(CPoint((int)std::lround(cx + std::cos(a1) * tipInner),
                                     (int)std::lround(cy + std::sin(a1) * tipInner)));
            canvas->fillPath(tipQuad, tip);
        }
    }
}

void SonicHaloEffect::drawAlbumArt(const AlbumArtVizContext &ctx, float cx, float cy, float artHalf,
                                   float artSpin) {
    if (!ctx.canvas) {
        return;
    }
    CRawGraph *canvas = ctx.canvas;
    const float drawSize = artHalf * 2.f;
    const float x = cx - artHalf;
    const float y = cy - artHalf;

    if (!ctx.albumArt || !ctx.albumArt->isValid() || ctx.albumArt->m_cx <= 0 ||
        ctx.albumArt->m_cy <= 0) {
        CColor disc = albumart_viz::paletteColor(ctx, 0.35f, 200);
        const int segs = 36;
        VecPoints poly;
        poly.reserve(segs);
        for (int i = 0; i < segs; ++i) {
            const float a = (float)(2.0 * M_PI * i / segs);
            poly.push_back(CPoint((int)std::lround(cx + std::cos(a) * artHalf),
                                  (int)std::lround(cy + std::sin(a) * artHalf)));
        }
        canvas->fillPath(poly, disc);
        return;
    }

    const int ix = (int)std::lround(x);
    const int iy = (int)std::lround(y);
    const int isize = (int)std::lround(drawSize);

    const CSFImage *drawImg = ctx.albumArt;
    CSFImage rotatedImg;
    if (std::fabs(artSpin) > 1e-4f) {
        auto rotated = rotateCircularArt(ctx.albumArt->getHandle(), artSpin);
        if (rotated) {
            rotatedImg.attach(rotated, ctx.scaleFactor);
            drawImg = &rotatedImg;
        }
    }

    if (drawImg->m_cx == isize && drawImg->m_cy == isize) {
        const_cast<CSFImage *>(drawImg)->blt(canvas, ix, iy, BPM_BLEND);
    } else {
        const_cast<CSFImage *>(drawImg)->stretchBlt(canvas, ix, iy, isize, isize, BPM_BLEND);
    }
}

void SonicHaloEffect::draw(const AlbumArtVizContext &ctx) {
    if (!ctx.canvas) {
        return;
    }
    const CRect &rc = ctx.bounds;
    if (rc.width() <= 4 || rc.height() <= 4) {
        return;
    }

    CRawGraph *canvas = ctx.canvas;
    const float cx = (float)(rc.left + rc.right) * 0.5f;
    const float cy = (float)(rc.top + rc.bottom) * 0.5f;
    const float minDim = (float)std::min(rc.width(), rc.height());
    const float artHalf = minDim * ctx.artRatio * 0.5f;
    const float baseR = artHalf * 1.08f;
    const float maxR = minDim * 0.48f;
    const float maxLen = std::max(6.f, maxR - baseR);

    {
        CColor wash = albumart_viz::paletteColor(ctx, 0.4f, 16);
        const int segs = 40;
        VecPoints poly;
        for (int i = 0; i < segs; ++i) {
            const float a = (float)(2.0 * M_PI * i / segs);
            const float r = maxR * (0.92f + ctx.ringSmooth * 0.08f);
            poly.push_back(CPoint((int)std::lround(cx + std::cos(a) * r),
                                  (int)std::lround(cy + std::sin(a) * r)));
        }
        canvas->fillPath(poly, wash);
    }

    const float spin = spinAngle(ctx);
    const float artSpin = (m_rotateMode == RotateMode::SyncAll) ? spin : 0.f;

    drawSpectrumRing(ctx, cx, cy, baseR, maxLen, spin);
    drawHaloRings(ctx, cx, cy, artHalf, maxR, spin);
    drawAlbumArt(ctx, cx, cy, artHalf, artSpin);
}
