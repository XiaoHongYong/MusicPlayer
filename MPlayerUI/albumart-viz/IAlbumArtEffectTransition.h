#pragma once

#include "AlbumArtVizContext.h"
#include "IAlbumArtEffect.h"

/**
 * 两个特效之间的转场：from / to 由宿主持有，转场不负责销毁。
 */
class IAlbumArtEffectTransition {
public:
    virtual ~IAlbumArtEffectTransition() = default;

    virtual cstr_t id() const = 0;

    virtual void begin(IAlbumArtEffect *from, IAlbumArtEffect *to, float durationSec) = 0;
    /** 推进进度；返回是否仍在进行。 */
    virtual bool update(float dtSec) = 0;
    virtual float progress() const = 0;
    virtual bool isFinished() const = 0;
    virtual void draw(const AlbumArtVizContext &ctx) = 0;
    virtual void cancel() = 0;

    virtual IAlbumArtEffect *fromEffect() const = 0;
    virtual IAlbumArtEffect *toEffect() const = 0;
};
