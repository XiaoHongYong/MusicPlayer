#pragma once

#include "AlbumArtVizContext.h"
#include "IAlbumArtVizHost.h"

/**
 * 封面可视化特效插件：可独立画出完整一帧。
 */
class IAlbumArtEffect {
public:
    virtual ~IAlbumArtEffect() = default;

    virtual cstr_t id() const = 0;
    virtual cstr_t name() const = 0;

    virtual void onAttach(IAlbumArtVizHost *host) { m_host = host; }
    virtual void onDetach() { m_host = nullptr; }

    virtual void onSize(const CRect &bounds) { (void)bounds; }
    virtual void onAlbumArtChanged(const AlbumArtVizContext &ctx) { (void)ctx; }

    /** 特效私有参数（如 RotateMode）；未识别返回 false。 */
    virtual bool setProperty(cstr_t name, cstr_t value) {
        (void)name;
        (void)value;
        return false;
    }

    virtual void update(const AlbumArtVizContext &ctx, float dtSec) {
        (void)ctx;
        (void)dtSec;
    }
    virtual void draw(const AlbumArtVizContext &ctx) = 0;

protected:
    IAlbumArtVizHost *m_host = nullptr;
};
