#pragma once

#include "../IAlbumArtEffect.h"

/**
 * 仅显示封面：原图保持长宽比缩放适配控件，不裁成圆形。
 */
class PlainCoverEffect : public IAlbumArtEffect {
public:
    cstr_t id() const override { return "plain-cover"; }
    cstr_t name() const override { return "Plain Cover"; }

    void draw(const AlbumArtVizContext &ctx) override;
};
