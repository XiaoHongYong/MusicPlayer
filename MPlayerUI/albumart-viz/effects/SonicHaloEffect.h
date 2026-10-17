#pragma once

#include "../IAlbumArtEffect.h"
#include "../../../GfxRaw/RawPen.h"

#include <vector>

/**
 * 径向频谱 + 光环 + 圆形封面；封面与可视化同步旋转。
 */
class SonicHaloEffect : public IAlbumArtEffect {
public:
    cstr_t id() const override { return "sonic-halo"; }
    cstr_t name() const override { return "Sonic Halo"; }

    void update(const AlbumArtVizContext &ctx, float dtSec) override;
    void draw(const AlbumArtVizContext &ctx) override;

private:
    float spinAngle(const AlbumArtVizContext &ctx) const;
    void updatePeakLevels(const AlbumArtVizContext &ctx);
    void drawHaloRings(const AlbumArtVizContext &ctx, float cx, float cy, float artHalf, float maxR,
                       float spin);
    void drawSpectrumRing(const AlbumArtVizContext &ctx, float cx, float cy, float baseR,
                          float maxLen, float spin);

    float m_breath = 0.f;
    std::vector<float> m_peakLevels;
    CRawPen m_pen;
};
