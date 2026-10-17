#include "AudioReactorEffect.h"
#include "../AlbumArtVizColorUtil.h"
#include "../AlbumArtVizDrawUtil.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kArtScale = 0.30f * 0.70f;
constexpr float kTrailSampleHz = 40.f;
constexpr float kSpinSpeed = 0.004f;
constexpr float kInitOrbitScale = 0.42f;
constexpr float kInitSpeedScale = 0.26f * 3.f; // 初速度 ×3
constexpr float kSizeSteerScale = 1.6f;         // 尺寸变化 → 转向
constexpr float kSparkLifeMin = 1.1f;
constexpr float kSparkLifeMax = 2.5f;
constexpr float kGlowGap = 3.f;          // 内反应环紧贴封面外侧
constexpr float kOuterRingScale = 1.85f; // 外环基准；厚度再 ×0.7
constexpr float kRingBandThicknessScale = 0.70f; // 内环带厚度减少 30%
constexpr float kParticleSpeedScale = 3.f;
// 喷射数量参考面积（约 Orbit-mini 主窗最小）；×2/3 后再 ×0.7（再减 30%）
constexpr float kSparkRefArea = 252.f * 403.f;
constexpr float kSparkMinDensity = (2.f / 3.f) * 0.7f;
// 粒子/微粒视觉尺寸再缩小 30%（速度仍只用 linearScale）
constexpr float kVizSizeScale = 0.7f;

/** 数量 ∝ 面积；速度/尺寸 ∝ √面积；拖尾数量另乘线性比抵消 traveled∝尺寸 */
void sparkEmitScales(const CRect &bounds, float &sprayScale, float &trailScale,
                     float &linearScale) {
    const float area =
        std::max(1.f, (float)bounds.width() * (float)bounds.height());
    const float areaRatio = area / kSparkRefArea;
    const float linearRatio = std::sqrt(std::max(1e-4f, areaRatio));
    sprayScale = std::clamp(areaRatio * kSparkMinDensity, 0.3f, 1.f);
    // traveled∝S，再 ×density·S → 总量 ∝ 面积
    trailScale = std::clamp(linearRatio * kSparkMinDensity, 0.3f, 1.f);
    // 发射速度随窗口线性尺寸缩放
    linearScale = std::clamp(linearRatio, 0.6f, 1.0f);
}

float bandAtAngle(const AlbumArtVizContext &ctx, float angle01) {
    if (!ctx.displayLevels || ctx.bandCount <= 0) {
        return 0.f;
    }
    const float pos = angle01 * (float)ctx.bandCount;
    const int i0 = ((int)pos) % ctx.bandCount;
    const int i1 = (i0 + 1) % ctx.bandCount;
    const float t = pos - std::floor(pos);
    return ctx.displayLevels[i0] * (1.f - t) + ctx.displayLevels[i1] * t;
}

float hash01(int n) {
    n = (n << 13) ^ n;
    const int nn = (n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff;
    return (float)nn / 2147483647.f;
}

CColor softNeon(const CColor &base, uint8_t alpha, float satAdd = 0.15f) {
    float h = 0, s = 0, l = 0;
    RGBToHLS(base.get(), h, s, l);
    s = std::clamp(s + satAdd * (1.f - s), 0.18f, 0.98f);
    l = std::clamp(l, 0.28f, 0.82f);
    CColor c;
    c.set(HLSToRGB(h, s, l));
    c.setAlpha(alpha);
    return c;
}

CColor ringNeon(const CColor &base, uint8_t alpha) {
    float h = 0, s = 0, l = 0;
    RGBToHLS(base.get(), h, s, l);
    // 反应环需要够亮够饱和，才能在暗底上看清
    s = std::clamp(s * 1.25f + 0.2f, 0.55f, 0.99f);
    l = std::clamp(l * 0.35f + 0.58f, 0.55f, 0.88f);
    CColor c;
    c.set(HLSToRGB(h, s, l));
    c.setAlpha(alpha);
    return c;
}

} // namespace

float AudioReactorEffect::segmentLevel(const AlbumArtVizContext &ctx, int segment) {
    if (!ctx.displayLevels || ctx.bandCount <= 0 || segment < 0 || segment >= kSegmentCount) {
        return 0.f;
    }
    const int n = ctx.bandCount;
    const int i0 = segment * n / kSegmentCount;
    const int i1 = std::max(i0 + 1, (segment + 1) * n / kSegmentCount);
    float sum = 0.f;
    int count = 0;
    for (int i = i0; i < i1 && i < n; ++i) {
        sum += ctx.displayLevels[i];
        ++count;
    }
    return count > 0 ? std::clamp(sum / (float)count, 0.f, 1.f) : 0.f;
}

CColor AudioReactorEffect::neonRole(const AlbumArtVizContext &ctx,
                                    albumart_viz::AlbumPaletteRole role, uint8_t alpha,
                                    float satBoost) const {
    CColor base = albumart_viz::paletteRole(ctx, role, 255);
    if (role == albumart_viz::AlbumPaletteRole::Light) {
        return ringNeon(base, alpha);
    }
    return softNeon(base, alpha, satBoost);
}

CColor AudioReactorEffect::neonRay(const AlbumArtVizContext &ctx, int bandIndex, int bandCount,
                                   uint8_t alpha) const {
    CColor base = albumart_viz::rayColor(ctx, bandIndex, bandCount, 255);
    if (!ctx.rayColors || ctx.rayColorCount <= 0) {
        base = albumart_viz::paletteRole(ctx, albumart_viz::AlbumPaletteRole::Accent, 255);
    }
    return softNeon(base, alpha, 0.2f);
}

CColor AudioReactorEffect::particleColor(const AlbumArtVizContext &ctx, int colorSlot, float hueT,
                                         uint8_t alpha) const {
    // 优先用封面提取的高差异粒子色盘（每粒子一色）
    if (ctx.particleColors && ctx.particleColorCount > 0) {
        return softNeon(albumart_viz::particlePaletteColor(ctx, colorSlot, 255), alpha, 0.22f);
    }
    if (colorSlot == 1) {
        return softNeon(albumart_viz::paletteRole(ctx, albumart_viz::AlbumPaletteRole::Accent, 255),
                        alpha, 0.35f);
    }
    if (colorSlot == 2) {
        return ringNeon(albumart_viz::paletteRole(ctx, albumart_viz::AlbumPaletteRole::Light, 255),
                        alpha);
    }
    const int bands = std::max(1, ctx.rayColorCount > 0 ? ctx.rayColorCount : 64);
    const int idx = (int)std::lround(hueT * (bands - 1));
    return neonRay(ctx, idx, bands, alpha);
}

void AudioReactorEffect::initParticleVelocity(Particle &p, float speed) {
    const float dir = hash01(p.seed + 11) * (float)(2.0 * M_PI);
    const float spd = std::max(18.f, speed * (0.85f + hash01(p.seed + 19) * 0.35f));
    p.vx = std::cos(dir) * spd;
    p.vy = std::sin(dir) * spd;
    p.wanderAngle = dir;
    p.wanderTimer = 0.2f + hash01(p.seed + 23) * 0.45f;
}

void AudioReactorEffect::integrateParticle(Particle &p, float cx, float cy, float left, float top,
                                           float right, float bottom, float dt, float sizeNorm,
                                           float artExcludeR, float innerRingR) {
    const float half = std::min(right - left, bottom - top) * 0.5f;
    if (half < 8.f) {
        return;
    }

    // 巡航速度带（×3）
    const float cruise = half * (0.24f + sizeNorm * 0.10f) * kParticleSpeedScale;
    const float minSpd = cruise * 0.75f;
    const float maxSpd = cruise * 1.4f;

    // 1) 尺寸变化 → 改朝向
    const float dSize = p.size - p.prevSize;
    if (std::fabs(dSize) > 0.04f) {
        const float side = (hash01(p.seed * 3 + (int)(p.size * 8.f)) > 0.5f) ? 1.f : -1.f;
        p.wanderAngle +=
            dSize * kSizeSteerScale * side + (hash01(p.seed + 7) - 0.5f) * 0.9f;
        p.asymAngle += dSize * 0.8f;
    }
    p.prevSize = p.size;

    // 2) 游荡：改朝向为主
    p.wanderTimer -= dt;
    if (p.wanderTimer <= 0.f) {
        p.wanderAngle += (hash01(p.seed + (int)(m_time * 60.f) + 9) - 0.5f) * (float)M_PI * 0.85f;
        p.wanderTimer = 0.22f + hash01(p.seed + (int)(m_time * 40.f)) * 0.55f;
    }
    const float n1 = hash01(p.seed + (int)(m_time * 19.f) + 3);
    p.wanderAngle += (n1 - 0.5f) * 1.8f * dt;

    const float margin = std::max(4.f, p.size * 0.55f);
    const float edgeDist =
        std::max(0.f, std::min({p.x - left - margin, right - margin - p.x, p.y - top - margin,
                                bottom - margin - p.y}));

    float dx = cx - p.x;
    float dy = cy - p.y;
    float r = std::sqrt(dx * dx + dy * dy) + 1e-3f;
    const float nx = dx / r; // 指向中心
    const float ny = dy / r;
    const float tx = -ny; // 切向
    const float ty = nx;

    auto shortestBlend = [](float &angle, float target, float amount) {
        float diff = target - angle;
        while (diff > (float)M_PI) {
            diff -= (float)(2.0 * M_PI);
        }
        while (diff < -(float)M_PI) {
            diff += (float)(2.0 * M_PI);
        }
        angle += diff * amount;
    };

    // 屏边：朝向扳回中心
    if (edgeDist < half * 0.18f) {
        const float toCenter = std::atan2(dy, dx);
        const float blend = std::clamp(1.f - edgeDist / (half * 0.18f), 0.f, 1.f);
        shortestBlend(p.wanderAngle, toCenter, blend * std::min(1.f, 4.f * dt));
    }

    // albumart 弹簧区：在碰到封面前提前外推（软弹簧，非硬反弹）
    const float restR = std::max(artExcludeR + p.size * 0.55f, 8.f);
    const float springStart = std::max(innerRingR + 6.f, restR + 18.f);
    float springT = 0.f;
    if (r < springStart) {
        springT = std::clamp((springStart - r) / std::max(1.f, springStart - restR), 0.f, 1.f);
        // 轻微带一点切向，避免直撞后原路弹回太死板
        const float tangSign = (p.vx * tx + p.vy * ty) >= 0.f ? 1.f : -1.f;
        const float outAng = std::atan2(-ny, -nx);
        const float tangAng = std::atan2(ty * tangSign, tx * tangSign);
        shortestBlend(p.wanderAngle, outAng * 0.65f + tangAng * 0.35f,
                      springT * springT * std::min(1.f, 3.5f * dt));
    }

    // 3) 期望速度
    const float pull = std::clamp((r - half * 0.28f) / (half * 0.55f), 0.f, 0.4f) * (1.f - springT);
    float dirX = std::cos(p.wanderAngle);
    float dirY = std::sin(p.wanderAngle);
    float dvx = dirX * (1.f - pull) + nx * pull;
    float dvy = dirY * (1.f - pull) + ny * pull;
    const float dlen = std::sqrt(dvx * dvx + dvy * dvy) + 1e-3f;
    dvx = dvx / dlen * cruise;
    dvy = dvy / dlen * cruise;

    const float steer = 1.f - std::exp(-3.2f * dt);
    p.vx += (dvx - p.vx) * steer;
    p.vy += (dvy - p.vy) * steer;

    // Hooke 弹簧：越靠近封面外推越强，并阻尼内向速度
    if (springT > 0.f) {
        const float ox = -nx;
        const float oy = -ny;
        const float compression = springStart - r;
        const float accel = (cruise * 10.f) * springT * springT + compression * 28.f;
        p.vx += ox * accel * dt;
        p.vy += oy * accel * dt;
        const float vIn = p.vx * nx + p.vy * ny; // >0 朝向中心
        if (vIn > 0.f) {
            const float damp = std::min(1.f, (2.8f + springT * 6.f) * dt);
            p.vx -= nx * vIn * damp;
            p.vy -= ny * vIn * damp;
        }
    }

    float spd = std::sqrt(p.vx * p.vx + p.vy * p.vy);
    if (spd < 1e-3f) {
        p.vx = dirX * minSpd;
        p.vy = dirY * minSpd;
        spd = minSpd;
    } else if (spd < minSpd) {
        const float s = minSpd / spd;
        p.vx *= s;
        p.vy *= s;
        spd = minSpd;
    } else if (spd > maxSpd) {
        const float s = maxSpd / spd;
        p.vx *= s;
        p.vy *= s;
        spd = maxSpd;
    }
    const float edgeVmax = std::clamp(edgeDist * 6.f + cruise * 0.55f, minSpd, maxSpd);
    if (spd > edgeVmax && spd > 1e-3f) {
        const float s = edgeVmax / spd;
        p.vx *= s;
        p.vy *= s;
    }

    p.x += p.vx * dt;
    p.y += p.vy * dt;
    p.asymAngle += dt * (0.5f + sizeNorm);

    // 安全底线：极少穿透时用弹簧位推回，不瞬间硬弹
    dx = cx - p.x;
    dy = cy - p.y;
    r = std::sqrt(dx * dx + dy * dy) + 1e-3f;
    if (r < restR) {
        const float ox = -dx / r;
        const float oy = -dy / r;
        const float push = (restR - r) * std::min(1.f, 8.f * dt);
        p.x += ox * push;
        p.y += oy * push;
        const float vIn = p.vx * (-ox) + p.vy * (-oy);
        if (vIn > 0.f) {
            p.vx -= (-ox) * vIn;
            p.vy -= (-oy) * vIn;
        }
        p.vx += ox * (restR - r) * 12.f * dt;
        p.vy += oy * (restR - r) * 12.f * dt;
    }

    if (p.x < left + margin) {
        p.x = left + margin;
        p.vx = std::fabs(p.vx) * 0.35f + cruise * 0.25f;
        p.wanderAngle = 0.f;
    } else if (p.x > right - margin) {
        p.x = right - margin;
        p.vx = -std::fabs(p.vx) * 0.35f - cruise * 0.25f;
        p.wanderAngle = (float)M_PI;
    }
    if (p.y < top + margin) {
        p.y = top + margin;
        p.vy = std::fabs(p.vy) * 0.35f + cruise * 0.25f;
        p.wanderAngle = (float)(M_PI * 0.5);
    } else if (p.y > bottom - margin) {
        p.y = bottom - margin;
        p.vy = -std::fabs(p.vy) * 0.35f - cruise * 0.25f;
        p.wanderAngle = (float)(M_PI * 1.5);
    }
}

void AudioReactorEffect::emitSparks(Particle &p, const AlbumArtVizContext &ctx, float dt,
                                    float traveled, float fromX, float fromY) {
    // 烟花由绑定通道触发：3×bassPulse×bassStrength / 2×beatPulse×beatStrength
    const bool useBass = (p.beatDrive == BeatDrive::Bass);
    const float drivePulse =
        std::clamp(useBass ? ctx.bassPulse : ctx.beatPulse, 0.f, 1.f);
    const float driveStrength =
        std::clamp(useBass ? ctx.bassStrength : ctx.beatStrength, 0.f, 1.f);
    const float burst =
        std::clamp(drivePulse * (0.55f + driveStrength * 0.45f), 0.f, 1.f);

    float sprayScale = 1.f;
    float trailScale = 1.f;
    float speedScale = 1.f;
    sparkEmitScales(ctx.bounds, sprayScale, trailScale, speedScale);

    auto pushSpark = [&](bool isTrail, float x, float y, float vx, float vy, float lifeScale) {
        if ((int)m_sparks.size() >= kSparkMax) {
            return;
        }
        Spark s;
        const float jitter = hash01(p.seed + (int)(m_time * 80.f) + (int)m_sparks.size());
        const float ja = jitter * (float)(2.0 * M_PI);
        const float jR = isTrail ? (2.5f + p.size * 0.8f) : p.size * 0.08f;
        s.x = x + std::cos(ja) * jR * jitter;
        s.y = y + std::sin(ja) * jR * (1.f - jitter);
        s.vx = vx;
        s.vy = vy;
        s.age = 0.f;
        s.life = (kSparkLifeMin + hash01(p.seed + 51 + (int)m_sparks.size()) *
                                      (kSparkLifeMax - kSparkLifeMin)) *
                 lifeScale;
        const float sz = std::max(0.8f, p.size);
        if (isTrail) {
            // 拖尾：缓慢外散变大
            s.size0 = sz * (0.20f + hash01(p.seed + 61) * 0.32f);
            s.size1 = s.size0 * (2.2f + hash01(p.seed + 67) * 2.0f);
        } else {
            // 烟花：存活约 3 秒；飞出后收束变小
            (void)lifeScale;
            s.size0 = sz * (0.45f + hash01(p.seed + 61) * 0.35f);
            s.size1 = s.size0 * (0.25f + hash01(p.seed + 67) * 0.2f);
            s.life = 2.85f + hash01(p.seed + 71) * 0.30f; // ≈3s
        }
        s.hueT = p.hueT;
        s.colorSlot = p.colorSlot;
        s.isTrail = isTrail;
        m_sparks.push_back(s);
    };

    // 1) 拖尾：固定密度/速度，仅随路程与窗口面积，不跟 beat/level
    if (traveled > 0.01f) {
        constexpr float kTrailEmitPerPx = 1.15f;
        constexpr float kTrailSpacing = 7.f;
        p.trailEmitAcc += traveled * kTrailEmitPerPx * trailScale;
        const int trailCap = std::max(1, (int)std::lround(6.f * trailScale));
        int n = (int)(p.trailEmitAcc / kTrailSpacing);
        if (n > trailCap) {
            n = trailCap;
        }
        if (n > 0) {
            p.trailEmitAcc -= (float)n * kTrailSpacing;
            const float cx = ((float)ctx.bounds.left + (float)ctx.bounds.right) * 0.5f;
            const float cy = ((float)ctx.bounds.top + (float)ctx.bounds.bottom) * 0.5f;
            const float pathDx = p.x - fromX;
            const float pathDy = p.y - fromY;
            float pathLen = std::sqrt(pathDx * pathDx + pathDy * pathDy);
            float nx = 0.f, ny = 1.f;
            if (pathLen > 1e-3f) {
                nx = -pathDy / pathLen;
                ny = pathDx / pathLen;
            }
            for (int i = 0; i < n && (int)m_sparks.size() < kSparkMax; ++i) {
                const float t = (i + 1.f) / (n + 1.f);
                const float sx = fromX + pathDx * t;
                const float sy = fromY + pathDy * t;
                const float side = (hash01(p.seed + i * 19 + (int)(m_time * 50.f)) - 0.5f) * 2.f;
                const float lat = side * 3.2f;

                float ox = sx - cx;
                float oy = sy - cy;
                const float olen = std::sqrt(ox * ox + oy * oy);
                if (olen < 1.f) {
                    const float a =
                        hash01(p.seed + i * 13 + (int)(m_time * 70.f)) * (float)(2.0 * M_PI);
                    ox = std::cos(a);
                    oy = std::sin(a);
                } else {
                    ox /= olen;
                    oy /= olen;
                }
                const float tang = (hash01(p.seed + i + 29) - 0.5f) * 1.2f;
                float dx = ox - oy * tang;
                float dy = oy + ox * tang;
                const float dlen = std::sqrt(dx * dx + dy * dy);
                const float inv = dlen > 1e-4f ? 1.f / dlen : 1.f;
                const float spd =
                    (12.f + hash01(p.seed + i + 41) * 10.f) * speedScale;
                pushSpark(true, sx + nx * lat, sy + ny * lat, dx * inv * spd, dy * inv * spd, 1.f);
            }
        }
        if (p.trailEmitAcc > kTrailSpacing * (float)trailCap) {
            p.trailEmitAcc = kTrailSpacing * (float)trailCap;
        }
    }

    // 2) 烟花：pulse 达阈后整圈环射（×5）+ 闪光中心再喷一层 + 中心闪光
    p.fireworkCooldown = std::max(0.f, p.fireworkCooldown - dt);
    if (burst > 0.28f && p.fireworkCooldown <= 0.f) {
        const float weight = useBass ? 1.15f : 0.9f;
        // 外环数量 ×5
        const int n = std::max(
            90, (int)std::lround((42.f + burst * 36.f) * 5.f * sprayScale * weight));
        // 喷射距离缩小 30%
        const float speedBase =
            (95.f + burst * 160.f + drivePulse * 50.f) * speedScale * 0.7f;
        const float ang0 = hash01(p.seed + (int)(m_time * 37.f)) * (float)(2.0 * M_PI);
        for (int i = 0; i < n && (int)m_sparks.size() < kSparkMax; ++i) {
            // 近似均匀铺满一周，再加一点抖动 → 烟花环
            const float a =
                ang0 + ((float)i + hash01(p.seed + i * 17) * 0.35f) / (float)n *
                           (float)(2.0 * M_PI);
            const float spd = speedBase * (0.65f + hash01(p.seed + i + 3) * 0.55f);
            pushSpark(false, p.x, p.y, std::cos(a) * spd, std::sin(a) * spd,
                      0.85f + burst * 0.35f);
        }

        // 闪光中心再喷一层：更密、稍慢、寿命更短，贴着闪光炸开
        const int nFlash = std::max(
            40, (int)std::lround((28.f + burst * 24.f) * 5.f * sprayScale * weight));
        const float flashSpdBase = speedBase * 0.45f;
        const float ang1 = ang0 + 0.37f;
        for (int i = 0; i < nFlash && (int)m_sparks.size() < kSparkMax; ++i) {
            const float a =
                ang1 + ((float)i + hash01(p.seed + i * 23 + 7) * 0.5f) / (float)nFlash *
                           (float)(2.0 * M_PI);
            const float spd = flashSpdBase * (0.4f + hash01(p.seed + i + 11) * 0.9f);
            pushSpark(false, p.x, p.y, std::cos(a) * spd, std::sin(a) * spd, 0.55f + burst * 0.25f);
        }

        FireworkFlash flash;
        flash.x = p.x;
        flash.y = p.y;
        flash.age = 0.f;
        flash.life = 0.16f + burst * 0.10f;
        // 闪光圆盘缩小 30%
        flash.size0 = p.size * (1.2f + burst * 0.8f) * 0.7f;
        flash.size1 = p.size * (5.5f + burst * 4.0f) * speedScale * 0.7f;
        // 白芯：1080p 满喷 = 9px，随窗口短边与喷射强度缩放
        {
            const float minDim =
                (float)std::min(ctx.bounds.width(), ctx.bounds.height());
            const float screenScale = std::clamp(minDim / 1080.f, 0.25f, 2.5f);
            flash.coreRadius = 9.f * screenScale * std::clamp(0.35f + burst * 0.65f, 0.35f, 1.f);
        }
        flash.colorSlot = p.colorSlot;
        m_flashes.push_back(flash);

        p.fireworkCooldown = useBass ? 0.14f : 0.11f;
    }
}

void AudioReactorEffect::updateSparks(float dt) {
    for (auto &s : m_sparks) {
        s.age += dt;
        // 拖尾：弱阻尼慢飘；烟花：中等阻尼，飞出后减速
        const float damp = s.isTrail ? 0.55f : 1.35f;
        s.vx *= (1.f - std::min(1.f, damp * dt));
        s.vy *= (1.f - std::min(1.f, damp * dt));
        s.x += s.vx * dt;
        s.y += s.vy * dt;
    }
    m_sparks.erase(std::remove_if(m_sparks.begin(), m_sparks.end(),
                                  [](const Spark &s) { return s.age >= s.life; }),
                   m_sparks.end());
}

void AudioReactorEffect::updateFireworkFlashes(float dt) {
    for (auto &f : m_flashes) {
        f.age += dt;
    }
    m_flashes.erase(std::remove_if(m_flashes.begin(), m_flashes.end(),
                                   [](const FireworkFlash &f) { return f.age >= f.life; }),
                    m_flashes.end());
}

void AudioReactorEffect::ensureFixedParticles(const CRect &bounds) {
    const float w = (float)bounds.width();
    const float h = (float)bounds.height();
    if (w <= 8.f || h <= 8.f) {
        return;
    }
    if (m_particlesInited) {
        return;
    }

    const float cx = (float)(bounds.left + bounds.right) * 0.5f;
    const float cy = (float)(bounds.top + bounds.bottom) * 0.5f;
    const float minDim = std::min(w, h);
    float sprayScale = 1.f, trailScale = 1.f, sizeScale = 1.f;
    sparkEmitScales(bounds, sprayScale, trailScale, sizeScale);
    (void)sprayScale;
    (void)trailScale;
    const float artHalf = minDim * kArtScale;
    const float orbitR = std::max(artHalf * 1.55f, minDim * kInitOrbitScale);
    const float speed = minDim * kInitSpeedScale;
    const float particleSize = kParticleSizePx * sizeScale * kVizSizeScale;

    m_sparks.clear();
    m_sparks.reserve(kSparkMax);

    for (int i = 0; i < kSegmentCount; ++i) {
        Particle &p = m_particles[i];
        p.segment = i;
        p.beatDrive = (i < kBassParticleCount) ? BeatDrive::Bass : BeatDrive::Pulse;
        p.seed = i * 97 + 13;
        const float a = (float)i / (float)kSegmentCount * (float)(2.0 * M_PI) +
                        hash01(p.seed) * 0.55f;
        p.x = cx + std::cos(a) * orbitR;
        p.y = cy + std::sin(a) * orbitR;
        p.size = particleSize;
        p.prevSize = particleSize;
        p.hueT = (float)i / (float)std::max(1, kSegmentCount - 1);
        // 与 particleColors[i] 一一对应，五粒子五色
        p.colorSlot = i;
        p.asymAngle = hash01(p.seed + 17) * (float)(2.0 * M_PI);
        p.trailEmitAcc = 0.f;
        p.fireworkCooldown = 0.f;
        p.trail.clear();
        p.trail.reserve(kTrailMaxPoints);
        initParticleVelocity(p, speed);
    }
    m_flashes.clear();
    m_particlesInited = true;
}

void AudioReactorEffect::update(const AlbumArtVizContext &ctx, float dtSec) {
    const float dt = std::clamp(dtSec, 0.f, 0.1f);
    const float rms = (ctx.hasFrame && ctx.frame) ? ctx.frame->rms : 0.f;

    // 呼吸：lowEnergy；kick 用 bassPulse（不含中频 beatPulse）
    const float target =
        ctx.lowEnergy * 0.09f + ctx.bassPulse * 0.04f * std::max(ctx.bassStrength, 0.35f) +
        rms * 0.015f;
    m_breath += (target - m_breath) * 0.2f;
    const float spinBoost =
        1.f + std::clamp(ctx.bassRhythmRate, 0.f, 3.f) * 0.12f * ctx.rhythmConfidence;
    m_spin += kSpinSpeed * spinBoost;
    m_time += dt;

    if (!m_ribbonsInited) {
        for (int i = 0; i < 4; ++i) {
            m_ribbons[i].phase = hash01(i + 11) * (float)(2.0 * M_PI);
            m_ribbons[i].speed = 0.6f + hash01(i + 21) * 1.4f;
            m_ribbons[i].amp = 0.08f + hash01(i + 31) * 0.12f;
            m_ribbons[i].paletteT = 0.55f + hash01(i + 41) * 0.35f;
        }
        m_ribbonsInited = true;
    }
    for (auto &rb : m_ribbons) {
        rb.phase += dt * rb.speed * (1.f + ctx.lowEnergy + ctx.onsetStrength * 0.5f) * 0.3f;
    }

    ensureFixedParticles(ctx.bounds);

    const float cx = (float)(ctx.bounds.left + ctx.bounds.right) * 0.5f;
    const float cy = (float)(ctx.bounds.top + ctx.bounds.bottom) * 0.5f;
    const float left = (float)ctx.bounds.left + 4.f;
    const float top = (float)ctx.bounds.top + 4.f;
    const float right = (float)ctx.bounds.right - 4.f;
    const float bottom = (float)ctx.bounds.bottom - 4.f;
    const float minDim = std::min(right - left, bottom - top);
    const float artHalf = minDim * kArtScale * (1.f + m_breath * 0.45f);
    const float innerRingR = artHalf + kGlowGap;
    const float artExcludeR = artHalf;

    float sprayScale = 1.f, trailScale = 1.f, sizeScale = 1.f;
    sparkEmitScales(ctx.bounds, sprayScale, trailScale, sizeScale);
    (void)sprayScale;
    (void)trailScale;
    const float particleSize = kParticleSizePx * sizeScale * kVizSizeScale;

    m_trailSampleAcc += dt;
    const bool sampleTrail = m_trailSampleAcc >= (1.f / kTrailSampleHz);
    if (sampleTrail) {
        m_trailSampleAcc = 0.f;
    }

    for (int i = 0; i < kSegmentCount; ++i) {
        Particle &p = m_particles[i];
        p.beatDrive = (i < kBassParticleCount) ? BeatDrive::Bass : BeatDrive::Pulse;
        p.size = particleSize;
        const float sizeNorm = 0.5f;

        const float ox = p.x;
        const float oy = p.y;
        integrateParticle(p, cx, cy, left, top, right, bottom, dt, sizeNorm, artExcludeR,
                           innerRingR);
        const float traveled = std::sqrt((p.x - ox) * (p.x - ox) + (p.y - oy) * (p.y - oy));
        emitSparks(p, ctx, dt, traveled, ox, oy);

        for (auto &tp : p.trail) {
            tp.age += dt;
        }
        p.trail.erase(std::remove_if(p.trail.begin(), p.trail.end(),
                                     [](const TrailPoint &tp) { return tp.age > kTrailDurationSec; }),
                      p.trail.end());
        if (sampleTrail) {
            TrailPoint tp;
            tp.x = p.x;
            tp.y = p.y;
            tp.age = 0.f;
            tp.size = p.size;
            p.trail.push_back(tp);
            if ((int)p.trail.size() > kTrailMaxPoints) {
                p.trail.erase(p.trail.begin(),
                              p.trail.begin() + ((int)p.trail.size() - kTrailMaxPoints));
            }
        }
    }

    updateSparks(dt);
    updateFireworkFlashes(dt);
}

void AudioReactorEffect::drawParticleBody(const AlbumArtVizContext &ctx, const Particle &p,
                                          uint8_t headA) {
    if (!ctx.canvas) {
        return;
    }
    // RGB 只算一次，各层只改 Alpha
    const CColor particleRgb = particleColor(ctx, p.colorSlot, p.hueT, 255);
    // 外晕：偏心 + 多层幂次衰减，形成非均匀径向渐变（不是平均晕）
    const float ox = std::cos(p.asymAngle) * p.size * 0.22f;
    const float oy = std::sin(p.asymAngle) * p.size * 0.22f;
    constexpr int kGlowLayers = 4;
    for (int i = kGlowLayers; i >= 1; --i) {
        const float t = (float)i / (float)kGlowLayers; // 1=外 → 内
        const float falloff = std::pow(1.f - t * 0.92f, 2.4f); // 外淡内亮
        const uint8_t a = (uint8_t)std::clamp((float)headA * falloff * 0.55f, 0.f, (float)headA);
        if (a < 6) {
            continue;
        }
        // 外层更偏心，内层更贴核，渐变方向感更强
        const float bias = t * t;
        const float gx = p.x + ox * bias;
        const float gy = p.y + oy * bias;
        const float r = p.size * (0.35f + t * 1.55f);
        CColor glow = particleRgb;
        glow.setAlpha(a);
        albumart_viz::fillSoftDisc(ctx.canvas, gx, gy, r, glow, 14);
    }
    // 亮核
    CColor core = particleRgb;
    core.setAlpha(headA);
    albumart_viz::fillSoftDisc(ctx.canvas, p.x, p.y, p.size * 0.42f, core, 12);
    CColor hot = particleRgb;
    hot.setAlpha((uint8_t)std::min(255, (int)headA + 40));
    albumart_viz::fillSoftDisc(ctx.canvas, p.x - ox * 0.15f, p.y - oy * 0.15f, p.size * 0.2f, hot,
                               10);
}

void AudioReactorEffect::drawSparks(const AlbumArtVizContext &ctx) {
    if (!ctx.canvas || m_sparks.empty()) {
        return;
    }
    for (const auto &s : m_sparks) {
        const float u = std::clamp(s.age / s.life, 0.f, 1.f);
        const float fade = (1.f - u) * (1.f - u);
        const float r = s.size0 + (s.size1 - s.size0) * u;
        // 烟花更亮、拖尾稍淡
        const float aMax = s.isTrail ? 180.f : 230.f;
        const uint8_t a = (uint8_t)std::clamp(aMax * fade, 0.f, aMax);
        if (a < 8) {
            continue;
        }
        CColor rgb = particleColor(ctx, s.colorSlot, s.hueT, 255);
        rgb.setAlpha(a);
        albumart_viz::fillSoftDisc(ctx.canvas, s.x, s.y, r, rgb, s.isTrail ? 8 : 10);
    }
}

void AudioReactorEffect::drawFireworkFlashes(const AlbumArtVizContext &ctx) {
    if (!ctx.canvas || m_flashes.empty()) {
        return;
    }
    for (const auto &f : m_flashes) {
        const float u = std::clamp(f.age / f.life, 0.f, 1.f);
        // 闪光：快速撑开后骤灭
        const float bloom = std::sin((1.f - u) * (float)M_PI);
        const float r = f.size0 + (f.size1 - f.size0) * (1.f - (1.f - u) * (1.f - u));
        CColor rgb = particleColor(ctx, f.colorSlot, 0.5f, 255);
        // 外晕：提亮
        CColor glow = rgb;
        glow.setAlpha((uint8_t)std::clamp(230.f * bloom, 0.f, 255.f));
        albumart_viz::fillSoftDisc(ctx.canvas, f.x, f.y, r, glow, 16);
        // 中层更亮
        CColor mid = rgb;
        mid.setAlpha((uint8_t)std::clamp(255.f * bloom, 0.f, 255.f));
        albumart_viz::fillSoftDisc(ctx.canvas, f.x, f.y, r * 0.55f, mid, 12);
        // 白芯：半径随喷射强度 / 相对 1080p 缩放（满喷约 9px）
        CColor core;
        core.set(255, 255, 255);
        core.setAlpha((uint8_t)std::clamp(255.f * bloom, 0.f, 255.f));
        albumart_viz::fillSoftDisc(ctx.canvas, f.x, f.y, f.coreRadius, core, 10);
    }
}

void AudioReactorEffect::drawEnergyCloud(const AlbumArtVizContext &ctx) {
    if (!ctx.canvas || !m_particlesInited) {
        return;
    }

    // 微粒 → 闪光 → 主粒子
    drawSparks(ctx);
    drawFireworkFlashes(ctx);

    for (const auto &p : m_particles) {
        const float level = segmentLevel(ctx, p.segment);
        const uint8_t headA = (uint8_t)std::clamp(
            180.f + level * 75.f + ctx.beatPulse * 35.f + ctx.onsetStrength * 25.f, 0.f, 255.f);
        const CColor particleRgb = particleColor(ctx, p.colorSlot, p.hueT, 255);

        if (p.trail.size() >= 2) {
            for (size_t i = 1; i < p.trail.size(); ++i) {
                const float life = 1.f - std::clamp(p.trail[i].age / kTrailDurationSec, 0.f, 1.f);
                const uint8_t a =
                    (uint8_t)std::clamp(headA * life * life * 0.85f, 0.f, (float)headA);
                if (a < 8) {
                    continue;
                }
                CColor trailClr = particleRgb;
                trailClr.setAlpha(a);
                const float trailSize = (p.trail[i - 1].size + p.trail[i].size) * 0.5f;
                const int penW =
                    std::max(1, (int)std::lround(trailSize * 0.42f * (0.35f + life * 0.65f)));
                m_pen.createSolidPen(penW, trailClr);
                ctx.canvas->setPen(m_pen);
                ctx.canvas->line(p.trail[i - 1].x, p.trail[i - 1].y, p.trail[i].x, p.trail[i].y);
            }
        }

        drawParticleBody(ctx, p, headA);
    }
}

void AudioReactorEffect::drawRibbons(const AlbumArtVizContext &ctx, float cx, float cy,
                                     float artHalf, float maxR) {
    if (!ctx.canvas) {
        return;
    }
    const float mid = (ctx.hasFrame && ctx.frame) ? ctx.frame->mid : 0.f;
    for (int ri = 0; ri < 4; ++ri) {
        const auto &rb = m_ribbons[ri];
        CColor clr =
            (ri % 2 == 0)
                ? neonRole(ctx, albumart_viz::AlbumPaletteRole::Accent,
                           (uint8_t)(45 + mid * 50 + ctx.onsetStrength * 55 + ctx.beatPulse * 25),
                           0.3f)
                : neonRay(ctx, ri * 17, 64, (uint8_t)(40 + mid * 45 + ctx.onsetStrength * 30));
        m_pen.createSolidPen(1, clr);
        ctx.canvas->setPen(m_pen);

        const int segs = 36;
        float prevX = 0, prevY = 0;
        for (int i = 0; i <= segs; ++i) {
            const float t = (float)i / (float)segs;
            const float a = rb.phase + m_spin * 0.5f + t * (float)(2.0 * M_PI) * (0.55f + ri * 0.08f);
            const float wavy = 1.f + rb.amp * std::sin(t * 8.f + m_spin * 3.f + ri);
            const float r = artHalf * 1.7f + (maxR - artHalf) * (0.25f + t * 0.55f) * wavy;
            const float x = cx + std::cos(a) * r;
            const float y = cy + std::sin(a) * r;
            if (i > 0) {
                ctx.canvas->line(prevX, prevY, x, y);
            }
            prevX = x;
            prevY = y;
        }
    }
}

void AudioReactorEffect::drawVinylGrooves(const AlbumArtVizContext &ctx, float cx, float cy,
                                          float artHalf, float vinylOuter) {
    if (!ctx.canvas) {
        return;
    }
    const float inner = artHalf * 1.06f;
    const int grooves = 14;
    // RGB 只转换一次，各槽只改 Alpha；圆周采样降至 40
    const CColor grooveRgb =
        neonRole(ctx, albumart_viz::AlbumPaletteRole::BgMid, 255, 0.2f);
    for (int i = 0; i < grooves; ++i) {
        const float t = (float)i / (float)(grooves - 1);
        const float r = inner + (vinylOuter - inner) * t;
        const uint8_t a = (uint8_t)(24 + (1.f - std::fabs(t - 0.5f) * 2.f) * 36);
        CColor clr = grooveRgb;
        clr.setAlpha(a);
        m_pen.createSolidPen(1, clr);
        albumart_viz::strokeCircle(ctx.canvas, m_pen, cx, cy, r, 40, m_spin);
    }
}

void AudioReactorEffect::drawReactorRing(const AlbumArtVizContext &ctx, float cx, float cy,
                                         float innerR, float outerR, float spin) {
    if (!ctx.canvas || outerR <= innerR + 1.f) {
        return;
    }
    const auto role = albumart_viz::AlbumPaletteRole::Light;
    // 48 段、每段画一次；复用路径对象，避免循环内堆分配
    const int segs = 48;
    CColor fillRgb = neonRole(ctx, role, 255, 0.45f);
    CColor coreRgb = neonRole(ctx, role, 255, 0.65f);
    VecPoints poly;
    poly.reserve(4);

    for (int i = 0; i < segs; ++i) {
        const float t0 = (float)i / (float)segs;
        const float t1 = (float)(i + 1) / (float)segs;
        const float level = (bandAtAngle(ctx, t0) + bandAtAngle(ctx, t1)) * 0.5f;
        const float a0 = spin + t0 * (float)(2.0 * M_PI) - (float)(M_PI * 0.5);
        const float a1 = spin + t1 * (float)(2.0 * M_PI) - (float)(M_PI * 0.5);
        const uint8_t a = (uint8_t)std::clamp(
            35.f + level * 150.f + ctx.beatPulse * 25.f + ctx.bassPulse * 20.f, 0.f, 200.f);
        if (a < 10) {
            continue;
        }
        const float cos0 = std::cos(a0);
        const float sin0 = std::sin(a0);
        const float cos1 = std::cos(a1);
        const float sin1 = std::sin(a1);
        poly.clear();
        poly.push_back(
            CPoint((int)std::lround(cx + cos0 * innerR), (int)std::lround(cy + sin0 * innerR)));
        poly.push_back(
            CPoint((int)std::lround(cx + cos1 * innerR), (int)std::lround(cy + sin1 * innerR)));
        poly.push_back(
            CPoint((int)std::lround(cx + cos1 * outerR), (int)std::lround(cy + sin1 * outerR)));
        poly.push_back(
            CPoint((int)std::lround(cx + cos0 * outerR), (int)std::lround(cy + sin0 * outerR)));
        CColor fill = fillRgb;
        fill.setAlpha(a);
        ctx.canvas->fillPath(poly, fill);
    }

    // 内环亮边：线宽固定，只改亮度
    constexpr int kInnerPenW = 3;
    for (int i = 0; i < segs; ++i) {
        const float t0 = (float)i / (float)segs;
        const float t1 = (float)(i + 1) / (float)segs;
        const float level = (bandAtAngle(ctx, t0) + bandAtAngle(ctx, t1)) * 0.5f;
        const float a0 = spin + t0 * (float)(2.0 * M_PI) - (float)(M_PI * 0.5);
        const float a1 = spin + t1 * (float)(2.0 * M_PI) - (float)(M_PI * 0.5);
        const uint8_t coreA = (uint8_t)std::clamp(
            110.f + level * 145.f + ctx.beatPulse * 35.f + ctx.bassPulse * 25.f, 0.f, 255.f);
        CColor core = coreRgb;
        core.setAlpha(coreA);
        m_pen.createSolidPen(kInnerPenW, core);
        ctx.canvas->setPen(m_pen);
        ctx.canvas->line(cx + std::cos(a0) * innerR, cy + std::sin(a0) * innerR,
                         cx + std::cos(a1) * innerR, cy + std::sin(a1) * innerR);
    }

    // 外环不再画辉光描边，外侧只保留频谱条
}

void AudioReactorEffect::drawSpectrumTicks(const AlbumArtVizContext &ctx, float cx, float cy,
                                           float ringR, float spin) {
    if (!ctx.canvas) {
        return;
    }
    // 外环：纯频谱条，无辉光填充
    const int ticks = std::clamp(ctx.bandCount > 0 ? ctx.bandCount : 64, 36, 96);
    const float maxLen = ringR * (0.32f + m_breath * 0.10f);

    for (int i = 0; i < ticks; ++i) {
        const float t = (float)i / (float)ticks;
        const float level = bandAtAngle(ctx, t);
        const float a = spin + t * (float)(2.0 * M_PI) - (float)(M_PI * 0.5);
        const float len = std::max(2.f, level * maxLen);
        const float cosA = std::cos(a);
        const float sinA = std::sin(a);
        CColor tick = neonRay(ctx, i, ticks, (uint8_t)(140 + level * 100));
        const int penW = std::max(1, (int)std::lround(1.f + level * 4.5f + m_breath * 2.f));
        m_pen.createSolidPen(penW, tick);
        ctx.canvas->setPen(m_pen);
        ctx.canvas->line(cx + cosA * ringR, cy + sinA * ringR, cx + cosA * (ringR + len),
                         cy + sinA * (ringR + len));
    }
}

void AudioReactorEffect::drawWingMarkers(const AlbumArtVizContext &ctx, float cx, float cy,
                                         float ringR, float spin) {
    if (!ctx.canvas) {
        return;
    }
    const float sides[2] = {0.f, (float)M_PI};
    CColor clr = neonRole(
        ctx, albumart_viz::AlbumPaletteRole::Accent,
        (uint8_t)std::clamp(200.f + ctx.beatPulse * 45.f + ctx.bassPulse * 25.f, 0.f, 255.f), 0.4f);
    m_pen.createSolidPen(std::max(1, (int)std::lround(1.5f + m_breath * 3.f)), clr);
    ctx.canvas->setPen(m_pen);

    for (float side : sides) {
        const float a = spin + side;
        const float dirX = std::cos(a);
        const float dirY = std::sin(a);
        const float nx = -dirY;
        const float ny = dirX;
        const float baseX = cx + dirX * (ringR + 5.f);
        const float baseY = cy + dirY * (ringR + 5.f);
        const float len = 9.f + m_breath * 12.f + ctx.beatPulse * 5.f + ctx.bassPulse * 4.f;
        const float gap = 4.f;
        for (int k = -1; k <= 1; ++k) {
            const float ox = nx * (k * gap);
            const float oy = ny * (k * gap);
            ctx.canvas->line(baseX + ox, baseY + oy, baseX + ox + dirX * len, baseY + oy + dirY * len);
        }
    }
}

void AudioReactorEffect::drawCoreRim(const AlbumArtVizContext &ctx, float cx, float cy,
                                     float artHalf) {
    if (!ctx.canvas) {
        return;
    }
    CColor rim = neonRole(
        ctx, albumart_viz::AlbumPaletteRole::Accent,
        (uint8_t)std::clamp(170.f + ctx.beatPulse * 55.f + ctx.bassPulse * 40.f, 0.f, 255.f), 0.4f);
    m_pen.createSolidPen(
        std::max(1, (int)std::lround(1.5f + m_breath * 5.f + ctx.bassPulse * 3.f)), rim);
    albumart_viz::strokeCircle(ctx.canvas, m_pen, cx, cy, artHalf * 1.01f, 64, m_spin);
}

void AudioReactorEffect::draw(const AlbumArtVizContext &ctx) {
    if (!ctx.canvas) {
        return;
    }
    const CRect &rc = ctx.bounds;
    if (rc.width() <= 4 || rc.height() <= 4) {
        return;
    }

    const float cx = (float)(rc.left + rc.right) * 0.5f;
    const float cy = (float)(rc.top + rc.bottom) * 0.5f;
    const float minDim = (float)std::min(rc.width(), rc.height());

    const float artHalf = minDim * kArtScale * (1.f + m_breath * 0.45f);
    const float vinylOuter = artHalf * 1.48f;
    // 内反应环贴封面；外环外扩后厚度再减 30%；中间填充；频谱从外环再外发
    const float innerR = artHalf + kGlowGap;
    const float outerBase = artHalf * kOuterRingScale * (1.f + m_breath * 0.08f);
    const float outerR = innerR + std::max(4.f, (outerBase - innerR) * kRingBandThicknessScale);
    const float tickR = outerR + 1.5f;
    const float maxR = minDim * 0.48f;
    const float spin = m_spin;

    {
        CColor wash =
            neonRole(ctx, albumart_viz::AlbumPaletteRole::BgDark, (uint8_t)(14 + ctx.ringSmooth * 18),
                     0.15f);
        albumart_viz::fillSoftDisc(ctx.canvas, cx, cy, maxR * (0.95f + m_breath * 0.2f), wash);
    }

    drawRibbons(ctx, cx, cy, artHalf, maxR);
    drawVinylGrooves(ctx, cx, cy, artHalf, vinylOuter);
    drawReactorRing(ctx, cx, cy, innerR, outerR, spin);
    drawSpectrumTicks(ctx, cx, cy, tickR, spin);
    drawWingMarkers(ctx, cx, cy, tickR, spin);
    albumart_viz::drawCircularAlbumArt(ctx, cx, cy, artHalf, 0.4f, spin);
    drawCoreRim(ctx, cx, cy, artHalf);
    // 粒子画在最上层，避免被封面/环遮住
    drawEnergyCloud(ctx);
}
