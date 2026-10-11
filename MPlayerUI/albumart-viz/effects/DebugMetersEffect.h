#pragma once

#include "../IAlbumArtEffect.h"
#include "../../../GfxRaw/FontInfoEx.h"
#include "../../../GfxRaw/RawBmpFont.h"
#include "../../../GfxRaw/RawPen.h"

/**
 * 调试用：柱状图显示节奏/起音平滑值（宿主 Context 字段）。
 * id: debug-meters
 */
class DebugMetersEffect : public IAlbumArtEffect {
public:
    cstr_t id() const override { return "debug-meters"; }
    cstr_t name() const override { return "Debug Meters"; }

    void update(const AlbumArtVizContext &ctx, float dtSec) override;
    void draw(const AlbumArtVizContext &ctx) override;

private:
    struct Meter {
        cstr_t label;
        float value;     // 柱高用 0..1
        float display;   // 文字用原始/显示值
        CColor color;
    };

    void ensureFont(float scaleFactor);
    void drawMeter(CRawGraph *canvas, const CRect &barRc, const Meter &m, float scale) const;

    CRawBmpFont m_font;
    float m_fontScale = 0.f;
    CRawPen m_pen;
    float m_peakHold[16]{};
};
