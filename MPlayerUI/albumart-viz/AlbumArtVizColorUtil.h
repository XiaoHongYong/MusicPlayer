#pragma once

#include "AlbumArtVizContext.h"

#include <algorithm>
#include <cmath>

namespace albumart_viz {

/**
 * 封面调色板角色（宿主 extractPalette 写入的 4 阶 + 边缘色环）。
 *
 * palette[0] BgDark   底色1 — 最暗，氛围/背景
 * palette[1] BgMid    底色2 — 中间调，次级背景
 * palette[2] Accent   亮色1 — 强调色（高饱和）
 * palette[3] Light    亮色2 — 高光/第二亮色
 * rayColors[N]        色环 — 沿封面边缘采色，按角度映射
 */
enum class AlbumPaletteRole {
    BgDark = 0,
    BgMid = 1,
    Accent = 2,
    Light = 3,
};

inline CColor lerpColor(const CColor &a, const CColor &b, float t, uint8_t alpha) {
    t = std::clamp(t, 0.f, 1.f);
    CColor c;
    c.set((int)std::lround(a.r() + (b.r() - a.r()) * t),
          (int)std::lround(a.g() + (b.g() - a.g()) * t),
          (int)std::lround(a.b() + (b.b() - a.b()) * t));
    c.setAlpha(alpha);
    return c;
}

inline CColor boostForViz(const CColor &c, float satBoost = 0.35f) {
    float h = 0, s = 0, l = 0;
    RGBToHLS(c.get(), h, s, l);
    s = std::clamp(s + satBoost * (1.f - s), 0.15f, 0.95f);
    if (s < 0.08f) {
        h = 28.f;
        s = 0.22f;
    }
    l = std::clamp(l * 0.55f + 0.38f, 0.28f, 0.72f);
    CColor out;
    out.set(HLSToRGB(h, s, l));
    out.setAlpha(c.getAlpha());
    return out;
}

inline CColor paletteColor(const AlbumArtVizContext &ctx, float t, uint8_t alpha) {
    if (!ctx.palette || ctx.paletteSize <= 0) {
        CColor c;
        c.set(180, 180, 180);
        c.setAlpha(alpha);
        return c;
    }
    t = std::clamp(t, 0.f, 1.f);
    if (ctx.paletteSize == 1) {
        CColor c = ctx.palette[0];
        c.setAlpha(alpha);
        return c;
    }
    const float scaled = t * (float)(ctx.paletteSize - 1);
    const int i0 = (int)scaled;
    const int i1 = std::min(ctx.paletteSize - 1, i0 + 1);
    return lerpColor(ctx.palette[i0], ctx.palette[i1], scaled - (float)i0, alpha);
}

inline CColor paletteRole(const AlbumArtVizContext &ctx, AlbumPaletteRole role, uint8_t alpha) {
    if (!ctx.palette || ctx.paletteSize <= 0) {
        return paletteColor(ctx, 0.5f, alpha);
    }
    const int idx = std::clamp((int)role, 0, ctx.paletteSize - 1);
    CColor c = ctx.palette[idx];
    c.setAlpha(alpha);
    return c;
}

inline CColor rayColor(const AlbumArtVizContext &ctx, int bandIndex, int bandCount, uint8_t alpha) {
    if (!ctx.rayColors || ctx.rayColorCount <= 0 || bandCount <= 0) {
        return paletteColor(ctx, 0.5f, alpha);
    }
    const float t = (float)bandIndex / (float)bandCount;
    const float idxF = t * (float)ctx.rayColorCount;
    const int i0 = ((int)idxF) % ctx.rayColorCount;
    const int i1 = (i0 + 1) % ctx.rayColorCount;
    return lerpColor(ctx.rayColors[i0], ctx.rayColors[i1], idxF - (float)(int)idxF, alpha);
}

} // namespace albumart_viz
