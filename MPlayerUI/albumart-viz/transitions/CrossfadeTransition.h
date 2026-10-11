#pragma once

#include "../IAlbumArtEffectTransition.h"

class CrossfadeTransition : public IAlbumArtEffectTransition {
public:
    cstr_t id() const override { return "crossfade"; }

    void begin(IAlbumArtEffect *from, IAlbumArtEffect *to, float durationSec) override;
    bool update(float dtSec) override;
    float progress() const override { return m_progress; }
    bool isFinished() const override { return m_finished; }
    void draw(const AlbumArtVizContext &ctx) override;
    void cancel() override;

    IAlbumArtEffect *fromEffect() const override { return m_from; }
    IAlbumArtEffect *toEffect() const override { return m_to; }

private:
    IAlbumArtEffect *m_from = nullptr;
    IAlbumArtEffect *m_to = nullptr;
    float m_durationSec = 0.6f;
    float m_elapsedSec = 0.f;
    float m_progress = 0.f;
    bool m_finished = true;
};
