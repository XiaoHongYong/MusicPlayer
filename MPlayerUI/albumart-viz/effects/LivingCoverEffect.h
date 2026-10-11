#pragma once

#include "../IAlbumArtEffect.h"
#include "../../../GfxRaw/RawPen.h"

#include <vector>

/**
 * Living Cover：封面呼吸 + 主色光晕 + 柔和波纹。
 * 适合默认听歌界面，反应柔和、不抢戏。
 */
class LivingCoverEffect : public IAlbumArtEffect {
public:
    cstr_t id() const override { return "living-cover"; }
    cstr_t name() const override { return "Living Cover"; }

    void update(const AlbumArtVizContext &ctx, float dtSec) override;
    void draw(const AlbumArtVizContext &ctx) override;

private:
    struct Ripple {
        float radius = 0.f;
        float alpha = 0.f;
        float speed = 1.f;
    };

    void spawnRipple(float artHalf);
    void drawAtmosphere(const AlbumArtVizContext &ctx, float cx, float cy, float maxR);
    void drawRipples(const AlbumArtVizContext &ctx, float cx, float cy);
    void drawWavyRim(const AlbumArtVizContext &ctx, float cx, float cy, float artHalf);

    float m_breath = 0.f;
    float m_wavePhase = 0.f;
    float m_rippleCooldown = 0.f;
    std::vector<Ripple> m_ripples;
    CRawPen m_pen;
};
