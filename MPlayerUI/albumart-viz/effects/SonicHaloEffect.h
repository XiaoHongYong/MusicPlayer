#pragma once

#include "../IAlbumArtEffect.h"
#include "../../../GfxRaw/RawPen.h"

#include <vector>

/**
 * 现有径向频谱 + 光环 + 圆形封面（从 CAlbumArtVisualizerCtrl 迁入）。
 */
class SonicHaloEffect : public IAlbumArtEffect {
public:
    enum class RotateMode {
        BarsOnly,
        SyncAll,
        None,
    };

    cstr_t id() const override { return "sonic-halo"; }
    cstr_t name() const override { return "Sonic Halo"; }

    bool setProperty(cstr_t name, cstr_t value) override;
    void update(const AlbumArtVizContext &ctx, float dtSec) override;
    void draw(const AlbumArtVizContext &ctx) override;

    void setRotateMode(RotateMode mode) { m_rotateMode = mode; }
    RotateMode rotateMode() const { return m_rotateMode; }

    static RotateMode rotateModeFromString(cstr_t s);
    static cstr_t rotateModeToString(RotateMode mode);

private:
    float spinAngle(const AlbumArtVizContext &ctx) const;
    void updatePeakLevels(const AlbumArtVizContext &ctx);
    void drawHaloRings(const AlbumArtVizContext &ctx, float cx, float cy, float artHalf, float maxR,
                       float spin);
    void drawSpectrumRing(const AlbumArtVizContext &ctx, float cx, float cy, float baseR,
                          float maxLen, float spin);
    void drawAlbumArt(const AlbumArtVizContext &ctx, float cx, float cy, float artHalf,
                      float artSpin);

    RotateMode m_rotateMode = RotateMode::BarsOnly;
    std::vector<float> m_peakLevels;
    CRawPen m_pen;
};
