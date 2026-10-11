#pragma once

/**
 * 特效对宿主的窄回调，避免依赖 CAlbumArtVisualizerCtrl。
 */
class IAlbumArtVizHost {
public:
    virtual ~IAlbumArtVizHost() = default;
    virtual void invalidateViz() = 0;
};
