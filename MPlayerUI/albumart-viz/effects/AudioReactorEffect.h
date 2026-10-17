#pragma once

#include "../IAlbumArtEffect.h"
#include "../AlbumArtVizColorUtil.h"
#include "../../../GfxRaw/RawPen.h"

#include <array>
#include <cstdint>
#include <vector>

/**
 * Audio Reactor：
 * 封面 / 频谱 / 反应环同步旋转；固定 5 粒子（3×bassPulse + 2×beatPulse）、约 0.5 秒拖尾；
 * 移动时按固定密度留拖尾微粒；beat/bass pulse 触发烟花式环射 + 中心闪光。
 */
class AudioReactorEffect : public IAlbumArtEffect {
public:
    cstr_t id() const override { return "audio-reactor"; }
    cstr_t name() const override { return "Audio Reactor"; }

    void update(const AlbumArtVizContext &ctx, float dtSec) override;
    void draw(const AlbumArtVizContext &ctx) override;

private:
    static constexpr int kSegmentCount = 5;
    static constexpr int kBassParticleCount = 3; // 其余为 beatPulse
    static constexpr float kTrailDurationSec = 0.5f;
    static constexpr int kTrailMaxPoints = 24; // ~40Hz * 0.5s
    static constexpr int kSparkMax = 12000;
    static constexpr float kParticleSizePx = 5.f;

    enum class BeatDrive : uint8_t {
        Bass,  // bassPulse + bassStrength
        Pulse, // beatPulse + beatStrength
    };

    struct TrailPoint {
        float x = 0.f;
        float y = 0.f;
        float age = 0.f;
        float size = 1.f;
    };

    struct Particle {
        int segment = 0;
        BeatDrive beatDrive = BeatDrive::Bass;
        float x = 0.f;
        float y = 0.f;
        float vx = 0.f;
        float vy = 0.f;
        float size = kParticleSizePx;
        float prevSize = kParticleSizePx;
        float hueT = 0.6f;
        int colorSlot = 0;
        float wanderAngle = 0.f;
        float wanderTimer = 0.f;
        float asymAngle = 0.f;
        float trailEmitAcc = 0.f;    // 拖尾微粒：按路程累计
        float fireworkCooldown = 0.f; // 烟花爆发冷却（秒）
        int seed = 0;
        std::vector<TrailPoint> trail;
    };

    /** 微粒：移动拖尾（慢速外散）或烟花喷射 */
    struct Spark {
        float x = 0.f;
        float y = 0.f;
        float vx = 0.f;
        float vy = 0.f;
        float age = 0.f;
        float life = 0.8f;
        float size0 = 1.5f;
        float size1 = 6.f;
        float hueT = 0.6f;
        int colorSlot = 0;
        bool isTrail = false; // true=拖尾；false=烟花
    };

    /** 烟花中心闪光 */
    struct FireworkFlash {
        float x = 0.f;
        float y = 0.f;
        float age = 0.f;
        float life = 0.22f;
        float size0 = 4.f;
        float size1 = 28.f;
        float coreRadius = 9.f; // 白芯半径：1080p 满喷约 9px，随喷射强度缩放
        int colorSlot = 0;
    };

    struct Ribbon {
        float phase = 0.f;
        float speed = 1.f;
        float amp = 1.f;
        float paletteT = 0.7f;
    };

    void ensureFixedParticles(const CRect &bounds);
    void initParticleVelocity(Particle &p, float speed);
    void integrateParticle(Particle &p, float cx, float cy, float left, float top, float right,
                           float bottom, float dt, float sizeNorm, float artExcludeR,
                           float innerRingR);
    void emitSparks(Particle &p, const AlbumArtVizContext &ctx, float dt, float traveled,
                    float fromX, float fromY);
    void updateSparks(float dt);
    void updateFireworkFlashes(float dt);
    CColor particleColor(const AlbumArtVizContext &ctx, int colorSlot, float hueT,
                         uint8_t alpha) const;
    static float segmentLevel(const AlbumArtVizContext &ctx, int segment);

    void drawEnergyCloud(const AlbumArtVizContext &ctx);
    void drawParticleBody(const AlbumArtVizContext &ctx, const Particle &p, uint8_t headA);
    void drawSparks(const AlbumArtVizContext &ctx);
    void drawFireworkFlashes(const AlbumArtVizContext &ctx);
    void drawRibbons(const AlbumArtVizContext &ctx, float cx, float cy, float artHalf, float maxR);
    void drawVinylGrooves(const AlbumArtVizContext &ctx, float cx, float cy, float artHalf,
                          float vinylOuter);
    void drawReactorRing(const AlbumArtVizContext &ctx, float cx, float cy, float innerR,
                         float outerR, float spin);
    void drawSpectrumTicks(const AlbumArtVizContext &ctx, float cx, float cy, float ringR,
                           float spin);
    void drawWingMarkers(const AlbumArtVizContext &ctx, float cx, float cy, float ringR,
                         float spin);
    void drawCoreRim(const AlbumArtVizContext &ctx, float cx, float cy, float artHalf);

    CColor neonRole(const AlbumArtVizContext &ctx, albumart_viz::AlbumPaletteRole role,
                    uint8_t alpha, float satBoost = 0.55f) const;
    CColor neonRay(const AlbumArtVizContext &ctx, int bandIndex, int bandCount,
                   uint8_t alpha) const;

    float m_breath = 0.f;
    float m_spin = 0.f;
    float m_time = 0.f;
    float m_trailSampleAcc = 0.f;

    std::array<Particle, kSegmentCount> m_particles{};
    bool m_particlesInited = false;
    std::vector<Spark> m_sparks;
    std::vector<FireworkFlash> m_flashes;

    Ribbon m_ribbons[4];
    bool m_ribbonsInited = false;
    CRawPen m_pen;
};
