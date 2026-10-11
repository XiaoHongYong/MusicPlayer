#include "LivingCoverEffect.h"
#include "../AlbumArtVizDrawUtil.h"

#include <algorithm>
#include <cmath>

void LivingCoverEffect::spawnRipple(float artHalf) {
    Ripple r;
    r.radius = artHalf * 1.05f;
    r.alpha = 0.55f;
    r.speed = 28.f + m_breath * 80.f;
    m_ripples.push_back(r);
    if (m_ripples.size() > 6) {
        m_ripples.erase(m_ripples.begin());
    }
}

void LivingCoverEffect::update(const AlbumArtVizContext &ctx, float dtSec) {
    const float dt = std::clamp(dtSec, 0.f, 0.1f);
    const float mid = (ctx.hasFrame && ctx.frame) ? ctx.frame->mid : 0.f;
    const float rms = (ctx.hasFrame && ctx.frame) ? ctx.frame->rms : 0.f;

    // 呼吸：lowEnergy；kick 用 bassPulse/bassStrength（不跟中频 beatPulse）
    const float energy = ctx.lowEnergy * 0.7f + mid * 0.15f + rms * 0.15f;
    const float target =
        energy > 0.02f
            ? (ctx.lowEnergy * 0.08f + ctx.bassPulse * 0.03f * std::max(ctx.bassStrength, 0.35f))
            : 0.f;
    const float follow = energy > 0.02f ? 0.16f : 0.06f;
    m_breath += (target - m_breath) * follow;

    // 波纹相位：中频 + 全频起音（踩镲等）加快
    m_wavePhase += dt * (0.7f + mid * 2.0f + ctx.onsetStrength * 1.8f + m_breath * 4.f);

    m_rippleCooldown = std::max(0.f, m_rippleCooldown - dt);
    // 涟漪：优先低频冲击；其次较强全频起音
    if (ctx.bassPulse > 0.4f && m_rippleCooldown <= 0.f) {
        const float minDim = (float)std::min(ctx.bounds.width(), ctx.bounds.height());
        spawnRipple(minDim * ctx.artRatio * 0.5f);
        m_rippleCooldown = 0.22f;
    } else if (ctx.beatPulse > 0.5f && ctx.bassPulse < 0.25f && m_rippleCooldown <= 0.f &&
               energy > 0.1f) {
        const float minDim = (float)std::min(ctx.bounds.width(), ctx.bounds.height());
        spawnRipple(minDim * ctx.artRatio * 0.5f);
        m_rippleCooldown = 0.5f;
    }

    for (auto &r : m_ripples) {
        r.radius += r.speed * dt * (1.f + ctx.bassPulse * 0.35f);
        r.alpha -= dt * 0.55f;
    }
    m_ripples.erase(std::remove_if(m_ripples.begin(), m_ripples.end(),
                                   [](const Ripple &r) { return r.alpha <= 0.02f; }),
                    m_ripples.end());
}

void LivingCoverEffect::drawAtmosphere(const AlbumArtVizContext &ctx, float cx, float cy,
                                       float maxR) {
    if (!ctx.canvas) {
        return;
    }
    const float pulse = 0.88f + ctx.ringSmooth * 0.1f + m_breath;
    struct Layer {
        float radiusScale;
        float paletteT;
        uint8_t alpha;
    };
    const Layer layers[] = {
        {1.00f, 0.25f, (uint8_t)(18 + ctx.ringSmooth * 22)},
        {0.78f, 0.45f, (uint8_t)(22 + ctx.ringSmooth * 28)},
        {0.58f, 0.65f, (uint8_t)(16 + m_breath * 120 + ctx.bassPulse * 40)},
    };
    for (const auto &layer : layers) {
        CColor wash = albumart_viz::paletteColor(ctx, layer.paletteT, layer.alpha);
        albumart_viz::fillSoftDisc(ctx.canvas, cx, cy, maxR * layer.radiusScale * pulse, wash);
    }
}

void LivingCoverEffect::drawRipples(const AlbumArtVizContext &ctx, float cx, float cy) {
    if (!ctx.canvas || m_ripples.empty()) {
        return;
    }
    for (const auto &r : m_ripples) {
        const uint8_t a = (uint8_t)std::clamp(r.alpha * 140.f, 0.f, 160.f);
        CColor clr = albumart_viz::paletteColor(ctx, 0.7f, a);
        m_pen.createSolidPen(std::max(1, (int)std::lround(1.2f + r.alpha * 2.5f)), clr);
        albumart_viz::strokeCircle(ctx.canvas, m_pen, cx, cy, r.radius, 56);
    }
}

void LivingCoverEffect::drawWavyRim(const AlbumArtVizContext &ctx, float cx, float cy,
                                    float artHalf) {
    if (!ctx.canvas) {
        return;
    }
    const float mid = (ctx.hasFrame && ctx.frame) ? ctx.frame->mid : 0.f;
    const float treble = (ctx.hasFrame && ctx.frame) ? ctx.frame->treble : 0.f;
    // 细振动：onset / treble；整体膨胀：breath
    const float amp =
        artHalf * (0.012f + mid * 0.03f + treble * 0.02f + ctx.onsetStrength * 0.03f +
                   m_breath * 0.4f);

    CColor rim =
        albumart_viz::paletteColor(ctx, 0.8f, (uint8_t)(90 + ctx.beatPulse * 70 + ctx.bassPulse * 50));
    m_pen.createSolidPen(std::max(2, (int)std::lround(1.8f + m_breath * 18.f + ctx.beatPulse * 2.f)),
                         rim);
    ctx.canvas->setPen(m_pen);

    const int segs = 64;
    for (int i = 0; i < segs; ++i) {
        const float a0 = (float)(2.0 * M_PI * i / segs);
        const float a1 = (float)(2.0 * M_PI * (i + 1) / segs);
        const float w0 = 1.f + (amp / artHalf) * std::sin(a0 * 5.f + m_wavePhase * 2.f);
        const float w1 = 1.f + (amp / artHalf) * std::sin(a1 * 5.f + m_wavePhase * 2.f);
        const float r0 = artHalf * 1.04f * w0;
        const float r1 = artHalf * 1.04f * w1;
        ctx.canvas->line(cx + std::cos(a0) * r0, cy + std::sin(a0) * r0,
                         cx + std::cos(a1) * r1, cy + std::sin(a1) * r1);
    }
}

void LivingCoverEffect::draw(const AlbumArtVizContext &ctx) {
    if (!ctx.canvas) {
        return;
    }
    const CRect &rc = ctx.bounds;
    if (rc.width() <= 4 || rc.height() <= 4) {
        return;
    }

    const float cx = (float)(rc.left + rc.right) * 0.5f;
    const float cy = (float)(rc.top + rc.bottom) * 0.5f;
    const float minDim = (float)std::min(rc.width(), rc.height());
    const float artHalfBase = minDim * ctx.artRatio * 0.5f;
    const float artHalf = artHalfBase * (1.f + m_breath);
    const float maxR = minDim * 0.48f;

    drawAtmosphere(ctx, cx, cy, maxR);
    drawRipples(ctx, cx, cy);
    drawWavyRim(ctx, cx, cy, artHalf);
    albumart_viz::drawCircularAlbumArt(ctx, cx, cy, artHalf, 0.45f);
}
