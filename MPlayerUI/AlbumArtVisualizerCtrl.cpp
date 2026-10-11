#include "MPlayerApp.h"
#include "AlbumArtVisualizerCtrl.h"
#include "../MPlayer/Player.h"
#include "../ImageLib/RawImageData.h"
#include "albumart-viz/AlbumArtVizColorUtil.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

UIOBJECT_CLASS_NAME_IMP(CAlbumArtVisualizerCtrl, "AlbumArtVisualizer")

RawImageDataPtr convertTo32BppRawImage(const RawImageDataPtr &src);

namespace {

constexpr float kAttack = 0.72f;
constexpr float kRelease = 0.32f;
constexpr float kTransitionSec = 0.65f;
constexpr cstr_t kProfileEffectKey = "AlbumArtVisualizerEffect";
constexpr cstr_t kProfileRotateKey = "AlbumArtVisualizerRotateMode";

RawImageDataPtr scaleCoverSquare(const RawImageDataPtr &src, int size) {
    if (!src || src->width <= 0 || src->height <= 0 || size <= 0) {
        return nullptr;
    }

    RawImageDataPtr dst = createRawImageData(size, size, src->bitCount);
    CRawImage imgSrc(src);
    CRawImage imgDst(dst);

    int xSrc = 0, ySrc = 0, wSrc = imgSrc.width(), hSrc = imgSrc.height();
    if (wSrc > hSrc) {
        xSrc = (wSrc - hSrc) / 2;
        wSrc = hSrc;
    } else if (hSrc > wSrc) {
        ySrc = (hSrc - wSrc) / 2;
        hSrc = wSrc;
    }

    imgSrc.stretchBlt(&imgDst, 0, 0, size, size, xSrc, ySrc, wSrc, hSrc, BPM_COPY | BPM_BILINEAR);
    return dst;
}

RawImageDataPtr makeCircularArt(const RawImageDataPtr &srcSquare) {
    if (!srcSquare) {
        return nullptr;
    }

    auto rgbaSrc = convertTo32BppRawImage(srcSquare);
    if (!rgbaSrc) {
        return nullptr;
    }
    auto rgba = duplicateRawImage(rgbaSrc.get());
    if (!rgba) {
        return nullptr;
    }

    const int size = rgba->width;
    const float cx = (size - 1) * 0.5f;
    const float cy = (size - 1) * 0.5f;
    const float radius = size * 0.5f;
    const float soft = std::max(1.5f, radius * 0.04f);

    for (int y = 0; y < size; ++y) {
        uint8_t *row = rgba->rowPtr(y);
        for (int x = 0; x < size; ++x) {
            const float dx = x - cx;
            const float dy = y - cy;
            const float dist = std::sqrt(dx * dx + dy * dy);
            float a = 1.f;
            if (dist > radius) {
                a = 0.f;
            } else if (dist > radius - soft) {
                a = (radius - dist) / soft;
            }
            uint8_t *px = row + x * 4;
            const uint8_t aa = (uint8_t)std::lround(a * 255.f);
            px[0] = (uint8_t)((px[0] * aa + 127) / 255);
            px[1] = (uint8_t)((px[1] * aa + 127) / 255);
            px[2] = (uint8_t)((px[2] * aa + 127) / 255);
            px[3] = aa;
        }
    }
    return rgba;
}

float colorSat(int r, int g, int b) {
    const float mx = (float)std::max(r, std::max(g, b));
    const float mn = (float)std::min(r, std::min(g, b));
    if (mx < 1.f) {
        return 0.f;
    }
    return (mx - mn) / mx;
}

float colorLum(int r, int g, int b) {
    return (0.2126f * r + 0.7152f * g + 0.0722f * b) / 255.f;
}

struct ColorBucket {
    float r = 0, g = 0, b = 0;
    int count = 0;
    float score = 0;
};

void readPixelBGR(const RawImageDataPtr &img, int x, int y, int &r, int &g, int &b) {
    x = std::clamp(x, 0, (int)img->width - 1);
    y = std::clamp(y, 0, (int)img->height - 1);
    const uint8_t *px = img->pixPtr(x, y);
    b = px[0];
    g = px[1];
    r = px[2];
}

} // namespace

CColor CAlbumArtVisualizerCtrl::boostForViz(const CColor &c, float satBoost) {
    return albumart_viz::boostForViz(c, satBoost);
}

CAlbumArtVisualizerCtrl::CAlbumArtVisualizerCtrl() {
    m_msgNeed = UO_MSG_WANT_LBUTTON;
    m_alive = std::make_shared<std::atomic<bool>>(true);
    m_fallbackColor.set(180, 180, 180);
    applyFallbackColor(m_fallbackColor);
    m_effectId = defaultAlbumArtEffectId();
    m_rotateMode = "bars";
    m_transitionId = defaultAlbumArtTransitionId();
}

CAlbumArtVisualizerCtrl::~CAlbumArtVisualizerCtrl() {
    *m_alive = false;
    if (m_transition) {
        m_transition->cancel();
        m_transition.reset();
    }
    if (m_nextEffect) {
        m_nextEffect->onDetach();
        m_nextEffect.reset();
    }
    if (m_effect) {
        m_effect->onDetach();
        m_effect.reset();
    }
    if (m_subscribedToAnalysis) {
        g_player.unsubscribeAudioAnalysis(this);
        m_subscribedToAnalysis = false;
    }
    if (m_pSkin) {
        m_pSkin->unregisterTimerObject(this);
    }
}

void CAlbumArtVisualizerCtrl::resetDefaultPalette() {
    for (int i = 0; i < kPaletteSize; ++i) {
        m_palette[i] = m_defaultPalette[i];
        m_palette[i].setAlpha(255);
    }
    for (int i = 0; i < kRayColorCount; ++i) {
        const float t = (float)i / (float)kRayColorCount;
        const float scaled = t * (kPaletteSize - 1);
        const int i0 = (int)scaled;
        const int i1 = std::min(kPaletteSize - 1, i0 + 1);
        m_rayColors[i] = albumart_viz::lerpColor(m_palette[i0], m_palette[i1], scaled - i0, 255);
    }
}

void CAlbumArtVisualizerCtrl::applyFallbackColor(const CColor &accent) {
    m_fallbackColor = accent;
    m_fallbackColor.setAlpha(255);

    float h = 0, s = 0, l = 0;
    RGBToHLS(accent.get(), h, s, l);

    m_defaultPalette[0].set(HLSToRGB(h, std::clamp(s * 0.35f, 0.f, 0.5f), 0.12f));
    m_defaultPalette[1].set(HLSToRGB(h, std::clamp(s * 0.75f, 0.f, 0.85f),
                                      std::clamp(l * 0.7f + 0.15f, 0.35f, 0.55f)));
    m_defaultPalette[2] = m_fallbackColor;
    m_defaultPalette[3].set(HLSToRGB(h, std::min(0.35f, s), std::min(0.92f, l * 0.35f + 0.62f)));
    for (int i = 0; i < kPaletteSize; ++i) {
        m_defaultPalette[i].setAlpha(255);
    }
    resetDefaultPalette();
}

bool CAlbumArtVisualizerCtrl::parsePalette(cstr_t szValue) {
    if (!szValue || !*szValue) {
        return false;
    }

    VecStrings parts;
    strSplit(szValue, ',', parts);
    std::vector<CColor> colors;
    colors.reserve(parts.size());
    for (auto &part : parts) {
        trimStr(part);
        if (part.empty()) {
            continue;
        }
        CColor c;
        getColorValue(c, part.c_str());
        c.setAlpha(255);
        colors.push_back(c);
    }
    if (colors.empty()) {
        return false;
    }

    if (colors.size() == 1) {
        applyFallbackColor(colors[0]);
        m_paletteFromXml = true;
        return true;
    }
    if (colors.size() == 2) {
        m_defaultPalette[0] = colors[0];
        m_defaultPalette[1] = colors[1];
        m_defaultPalette[2] = colors[1];
        m_defaultPalette[3] = colors[1];
        {
            float h = 0, s = 0, l = 0;
            RGBToHLS(colors[1].get(), h, s, l);
            m_defaultPalette[3].set(HLSToRGB(h, std::min(0.3f, s), 0.9f));
            m_defaultPalette[3].setAlpha(255);
        }
    } else if (colors.size() == 3) {
        m_defaultPalette[0] = colors[0];
        m_defaultPalette[1] = colors[1];
        m_defaultPalette[2] = colors[2];
        float h = 0, s = 0, l = 0;
        RGBToHLS(colors[2].get(), h, s, l);
        m_defaultPalette[3].set(HLSToRGB(h, std::min(0.3f, s), 0.9f));
        m_defaultPalette[3].setAlpha(255);
    } else {
        for (int i = 0; i < kPaletteSize; ++i) {
            m_defaultPalette[i] = colors[i];
            m_defaultPalette[i].setAlpha(255);
        }
    }
    m_fallbackColor = m_defaultPalette[2];
    m_paletteFromXml = true;
    resetDefaultPalette();
    return true;
}

void CAlbumArtVisualizerCtrl::loadRotateMode() {
    m_rotateMode = g_profile.getString(SZ_SECT_UI, kProfileRotateKey, m_rotateMode.c_str());
    forwardRotateModeToEffect();
}

void CAlbumArtVisualizerCtrl::saveRotateMode() const {
    g_profile.writeString(SZ_SECT_UI, kProfileRotateKey, m_rotateMode.c_str());
}

void CAlbumArtVisualizerCtrl::loadEffectId() {
    m_effectId = g_profile.getString(SZ_SECT_UI, kProfileEffectKey, m_effectId.c_str());
}

void CAlbumArtVisualizerCtrl::saveEffectId() const {
    g_profile.writeString(SZ_SECT_UI, kProfileEffectKey, m_effectId.c_str());
}

void CAlbumArtVisualizerCtrl::forwardRotateModeToEffect() {
    if (m_effect) {
        m_effect->setProperty("RotateMode", m_rotateMode.c_str());
    }
    if (m_nextEffect) {
        m_nextEffect->setProperty("RotateMode", m_rotateMode.c_str());
    }
}

void CAlbumArtVisualizerCtrl::ensureEffect(cstr_t id) {
    if (!id || !*id) {
        id = defaultAlbumArtEffectId();
    }
    if (m_effect && strcasecmp(m_effect->id(), id) == 0) {
        return;
    }
    if (m_effect) {
        m_effect->onDetach();
    }
    m_effect = createAlbumArtEffect(id);
    m_effectId = m_effect->id();
    m_effect->onAttach(this);
    m_effect->onSize(m_rcObj);
    m_effect->setProperty("RotateMode", m_rotateMode.c_str());
    notifyEffectsAlbumArtChanged();
}

void CAlbumArtVisualizerCtrl::finishTransition() {
    if (!m_transition) {
        return;
    }
    IAlbumArtEffect *to = m_transition->toEffect();
    IAlbumArtEffect *from = m_transition->fromEffect();
    m_transition->cancel();
    m_transition.reset();

    if (to && m_nextEffect.get() == to) {
        if (m_effect && m_effect.get() == from) {
            m_effect->onDetach();
        }
        m_effect = std::move(m_nextEffect);
        m_effectId = m_effect->id();
        saveEffectId();
    } else if (m_nextEffect) {
        m_nextEffect->onDetach();
        m_nextEffect.reset();
    }
}

void CAlbumArtVisualizerCtrl::requestEffect(cstr_t id) {
    if (!id || !*id) {
        return;
    }
    if (m_transition && !m_transition->isFinished()) {
        finishTransition();
    }
    if (m_effect && strcasecmp(m_effect->id(), id) == 0) {
        return;
    }

    auto next = createAlbumArtEffect(id);
    next->onAttach(this);
    next->onSize(m_rcObj);
    next->setProperty("RotateMode", m_rotateMode.c_str());
    {
        AlbumArtVizContext ctx = buildContext(nullptr, nullptr, false);
        next->onAlbumArtChanged(ctx);
    }

    if (!m_effect) {
        m_effect = std::move(next);
        m_effectId = m_effect->id();
        saveEffectId();
        invalidate();
        return;
    }

    m_nextEffect = std::move(next);
    m_transition = createAlbumArtEffectTransition(m_transitionId.c_str());
    m_transition->begin(m_effect.get(), m_nextEffect.get(), kTransitionSec);
    invalidate();
}

void CAlbumArtVisualizerCtrl::cycleEffect() {
    const auto &ids = albumArtEffectIds();
    if (ids.empty()) {
        return;
    }
    size_t idx = 0;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (m_effect && strcasecmp(m_effect->id(), ids[i]) == 0) {
            idx = (i + 1) % ids.size();
            break;
        }
    }
    requestEffect(ids[idx]);
}

void CAlbumArtVisualizerCtrl::notifyEffectsAlbumArtChanged() {
    AlbumArtVizContext ctx = buildContext(nullptr, nullptr, false);
    if (m_effect) {
        m_effect->onAlbumArtChanged(ctx);
    }
    if (m_nextEffect) {
        m_nextEffect->onAlbumArtChanged(ctx);
    }
}

AlbumArtVizContext CAlbumArtVisualizerCtrl::buildContext(CRawGraph *canvas,
                                                         const AudioAnalysisFrame *frame,
                                                         bool hasFrame) {
    AlbumArtVizContext ctx;
    ctx.canvas = canvas;
    ctx.bounds = m_rcObj;
    ctx.scaleFactor = m_pSkin ? m_pSkin->getScaleFactor() : 1.f;
    ctx.albumArt = m_imgArt.isValid() ? &m_imgArt : nullptr;
    ctx.hasAlbumArt = m_hasAlbumArt && m_imgArt.isValid();
    ctx.palette = m_palette;
    ctx.paletteSize = kPaletteSize;
    ctx.rayColors = m_rayColors;
    ctx.rayColorCount = kRayColorCount;
    ctx.frame = frame;
    ctx.hasFrame = hasFrame;
    ctx.displayLevels = m_displayLevels.empty() ? nullptr : m_displayLevels.data();
    ctx.bandCount = (int)m_displayLevels.size();
    ctx.artRatio = m_artRatio;
    ctx.lowEnergy = m_lowEnergySmooth;
    ctx.lowOnset = m_lowOnsetFlash;
    ctx.lowOnsetStrength = m_lowOnsetStrengthSmooth;
    ctx.bassPulse = m_bassPulseFlash;
    ctx.bassStrength = m_bassStrengthSmooth;
    ctx.beatPulse = m_beatPulseFlash;
    ctx.beatStrength = m_beatStrengthSmooth;
    ctx.onsetStrength = m_onsetStrengthSmooth;
    ctx.bassRhythmRate = m_bassRhythmRateSmooth;
    ctx.rhythmConfidence = m_rhythmConfidenceSmooth;
    ctx.beatFlash = m_beatPulseFlash;
    ctx.ringSmooth = m_ringSmooth;
    ctx.nowSec = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
                     .count();
    return ctx;
}

float CAlbumArtVisualizerCtrl::consumeDtSec() {
    const auto now = std::chrono::steady_clock::now();
    float dt = 1.f / 60.f;
    if (m_hasLastDrawTime) {
        dt = std::chrono::duration<float>(now - m_lastDrawTime).count();
        dt = std::clamp(dt, 0.f, 0.1f);
    }
    m_lastDrawTime = now;
    m_hasLastDrawTime = true;
    return dt;
}

bool CAlbumArtVisualizerCtrl::onLButtonUp(uint32_t nFlags, CPoint point) {
    if (!m_rcObj.ptInRect(point)) {
        return false;
    }
    cycleEffect();
    return true;
}

void CAlbumArtVisualizerCtrl::onCreate() {
    CUIObject::onCreate();

    registerHandler(MPlayerApp::getEventsDispatcher(), ET_PLAYER_CUR_MEDIA_CHANGED,
                    ET_PLAYER_CUR_MEDIA_INFO_CHANGED);
    m_pSkin->registerTimerObject(this, 20 * 1000);

    loadRotateMode();
    loadEffectId();
    ensureEffect(m_effectId.c_str());
    syncAnalysisSubscription();
    updateAlbumArt();
}

void CAlbumArtVisualizerCtrl::onSize() {
    CUIObject::onSize();
    if (m_imgArtOrg.isValid()) {
        rebuildAlbumArtImage(m_imgArtOrg.getHandle());
    }
    if (m_effect) {
        m_effect->onSize(m_rcObj);
    }
    if (m_nextEffect) {
        m_nextEffect->onSize(m_rcObj);
    }
}

void CAlbumArtVisualizerCtrl::onTimer(int nId) {
    RawImageDataPtr image = m_mediaAlbumArt.loadNext();
    if (image) {
        m_imgArtOrg.attach(image);
        extractPalette(image);
        sampleRayColors(image);
        rebuildAlbumArtImage(image);
        notifyEffectsAlbumArtChanged();
        invalidate();
    }
}

void CAlbumArtVisualizerCtrl::onEvent(const IEvent *pEvent) {
    if (pEvent->eventType == ET_PLAYER_CUR_MEDIA_CHANGED ||
        pEvent->eventType == ET_PLAYER_CUR_MEDIA_INFO_CHANGED) {
        m_pSkin->unregisterTimerObject(this);
        m_pSkin->registerTimerObject(this, 20 * 1000);
        updateAlbumArt();
        invalidate();
    }
}

void CAlbumArtVisualizerCtrl::syncAnalysisSubscription() {
    if (m_subscribedToAnalysis) {
        return;
    }
    AudioAnalysisOptions opt;
    opt.bandCount = 64;
    opt.outputRate = 30;
    opt.fftSize = 2048;
    opt.hopSize = 512;
    g_player.setAudioAnalysisOptions(opt);
    g_player.subscribeAudioAnalysis(this);
    m_subscribedToAnalysis = true;
}

void CAlbumArtVisualizerCtrl::onAudioAnalysisFrame(const AudioAnalysisFrame &frame) {
    {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        m_frame = frame;
        m_hasFrame = true;
    }
    scheduleInvalidate();
}

void CAlbumArtVisualizerCtrl::scheduleInvalidate() {
    bool expected = false;
    if (!m_invalidatePending.compare_exchange_strong(expected, true)) {
        return;
    }

    auto alive = m_alive;
    MPlayerApp::getEventsDispatcher()->postExecInUIThread([this, alive]() {
        m_invalidatePending = false;
        if (!alive || !*alive) {
            return;
        }
        invalidate();
    });
}

void CAlbumArtVisualizerCtrl::updateAlbumArt() {
    m_mediaAlbumArt.reset();
    m_imgArt.detach();
    m_imgArtOrg.detach();
    m_hasAlbumArt = false;

    auto image = m_mediaAlbumArt.loadNext();
    if (image) {
        m_imgArtOrg.attach(image);
        extractPalette(image);
        sampleRayColors(image);
        rebuildAlbumArtImage(image);
        m_hasAlbumArt = true;
        notifyEffectsAlbumArtChanged();
        return;
    }

    resetDefaultPalette();
    if (!m_strImgFile.empty()) {
        m_imgArtOrg.loadFromSRM(m_pSkin, m_strImgFile.c_str());
        if (m_imgArtOrg.isValid()) {
            extractPalette(m_imgArtOrg.getHandle());
            sampleRayColors(m_imgArtOrg.getHandle());
            rebuildAlbumArtImage(m_imgArtOrg.getHandle());
            m_hasAlbumArt = true;
        }
    }
    notifyEffectsAlbumArtChanged();
}

void CAlbumArtVisualizerCtrl::rebuildAlbumArtImage(const RawImageDataPtr &src) {
    m_imgArt.detach();
    if (!src || m_rcObj.width() <= 0 || m_rcObj.height() <= 0) {
        return;
    }

    const float scale = m_pSkin ? m_pSkin->getScaleFactor() : 1.f;
    const int minDim = std::min(m_rcObj.width(), m_rcObj.height());
    const int artPx = std::max(32, (int)std::lround(minDim * m_artRatio * scale));

    auto square = scaleCoverSquare(src, artPx);
    auto circle = makeCircularArt(square);
    if (circle) {
        m_imgArt.attach(circle, scale);
        m_hasAlbumArt = true;
    }
}

void CAlbumArtVisualizerCtrl::sampleRayColors(const RawImageDataPtr &src) {
    if (!src || src->width < 4 || src->height < 4) {
        return;
    }
    auto rgba = convertTo32BppRawImage(src);
    if (!rgba) {
        return;
    }

    const float cx = (rgba->width - 1) * 0.5f;
    const float cy = (rgba->height - 1) * 0.5f;
    const float radius = std::min(rgba->width, rgba->height) * 0.5f;

    for (int i = 0; i < kRayColorCount; ++i) {
        const float a = (float)i / (float)kRayColorCount * (float)(2.0 * M_PI) - (float)(M_PI * 0.5);
        const float dx = std::cos(a);
        const float dy = std::sin(a);

        float sumR = 0, sumG = 0, sumB = 0;
        int samples = 0;
        for (int k = 0; k < 3; ++k) {
            const float t = radius * (0.72f + 0.08f * k);
            int r = 0, g = 0, b = 0;
            readPixelBGR(rgba, (int)std::lround(cx + dx * t), (int)std::lround(cy + dy * t), r, g,
                         b);
            sumR += r;
            sumG += g;
            sumB += b;
            ++samples;
        }
        CColor raw;
        raw.set((int)(sumR / samples), (int)(sumG / samples), (int)(sumB / samples));
        // 色环保留封面边缘原色相，只轻度抬饱和，避免与调色板被拉成同色
        float h = 0, s = 0, l = 0;
        RGBToHLS(raw.get(), h, s, l);
        s = std::clamp(s * 1.2f + 0.08f, 0.12f, 0.95f);
        l = std::clamp(l * 0.9f + 0.08f, 0.22f, 0.82f);
        raw.set(HLSToRGB(h, s, l));
        raw.setAlpha(255);
        m_rayColors[i] = raw;
    }
}

namespace {

float hueDistanceDeg(float a, float b) {
    float d = std::fabs(a - b);
    if (d > 180.f) {
        d = 360.f - d;
    }
    return d;
}

CColor colorFromBucket(const ColorBucket &bk) {
    return CColor(RGB((int)bk.r, (int)bk.g, (int)bk.b));
}

} // namespace

// 写入 4 阶调色板（尽量拉开色相）：
//   [0] BgDark 底色1  [1] BgMid 底色2  [2] Accent 亮色1  [3] Light 亮色2
// 边缘色环见 sampleRayColors → m_rayColors。
void CAlbumArtVisualizerCtrl::extractPalette(const RawImageDataPtr &src) {
    resetDefaultPalette();
    if (!src || src->width < 4 || src->height < 4) {
        return;
    }

    auto rgba = convertTo32BppRawImage(src);
    if (!rgba) {
        return;
    }

    constexpr int kHueBins = 12;
    constexpr int kLumBins = 4;
    ColorBucket buckets[kHueBins * kLumBins];
    ColorBucket bestSat;
    float bestSatScore = -1.f;
    float avgR = 0, avgG = 0, avgB = 0;
    int avgCount = 0;

    const int stepX = std::max(1, rgba->width / 56);
    const int stepY = std::max(1, rgba->height / 56);

    for (int y = 0; y < rgba->height; y += stepY) {
        for (int x = 0; x < rgba->width; x += stepX) {
            int r = 0, g = 0, b = 0;
            readPixelBGR(rgba, x, y, r, g, b);
            const float lum = colorLum(r, g, b);
            if (lum < 0.04f || lum > 0.96f) {
                continue;
            }

            avgR += r;
            avgG += g;
            avgB += b;
            ++avgCount;

            const float sat = colorSat(r, g, b);
            if (sat > bestSatScore) {
                bestSatScore = sat;
                bestSat.r = (float)r;
                bestSat.g = (float)g;
                bestSat.b = (float)b;
                bestSat.count = 1;
            }

            float hue = 0, s = 0, l = 0;
            RGBToHLS(RGB(r, g, b), hue, s, l);
            int hi = (int)std::floor(hue / 360.f * kHueBins);
            hi = std::clamp(hi, 0, kHueBins - 1);
            int li = std::clamp((int)std::floor(lum * kLumBins), 0, kLumBins - 1);
            ColorBucket &bk = buckets[hi * kLumBins + li];
            bk.r += r;
            bk.g += g;
            bk.b += b;
            bk.count += 1;
        }
    }

    if (avgCount <= 0) {
        return;
    }

    CColor avg;
    avg.set((int)(avgR / avgCount), (int)(avgG / avgCount), (int)(avgB / avgCount));

    std::vector<ColorBucket> ranked;
    for (auto &bk : buckets) {
        if (bk.count < 2) {
            continue;
        }
        bk.r /= bk.count;
        bk.g /= bk.count;
        bk.b /= bk.count;
        const float sat = colorSat((int)bk.r, (int)bk.g, (int)bk.b);
        bk.score = (float)bk.count * (0.35f + sat * 1.8f);
        ranked.push_back(bk);
    }
    std::sort(ranked.begin(), ranked.end(),
              [](const ColorBucket &a, const ColorBucket &b) { return a.score > b.score; });

    // 底色1：压暗平均色
    CColor dark = avg;
    {
        float h = 0, s = 0, l = 0;
        RGBToHLS(dark.get(), h, s, l);
        dark.set(HLSToRGB(h, std::clamp(s * 0.55f, 0.f, 0.55f), std::max(0.10f, l * 0.35f)));
    }

    // 底色2：次亮/中频桶，偏中间明度
    CColor mid = boostForViz(avg, 0.25f);
    for (const auto &bk : ranked) {
        const float lum = colorLum((int)bk.r, (int)bk.g, (int)bk.b);
        if (lum > 0.25f && lum < 0.7f) {
            mid = boostForViz(colorFromBucket(bk), 0.3f);
            break;
        }
    }

    // 亮色1：最高饱和点
    CColor accent = mid;
    if (bestSat.count > 0 && bestSatScore > 0.08f) {
        accent = boostForViz(colorFromBucket(bestSat), 0.55f);
    } else if (!ranked.empty()) {
        accent = boostForViz(colorFromBucket(ranked.front()), 0.5f);
    }
    {
        float h = 0, s = 0, l = 0;
        RGBToHLS(accent.get(), h, s, l);
        accent.set(HLSToRGB(h, std::clamp(s + 0.12f, 0.35f, 0.98f), std::clamp(l, 0.4f, 0.7f)));
    }

    // 亮色2：与亮色1色相尽量拉开（≥50°），再提亮；找不到则色相旋转 +72°
    float accentH = 0, accentS = 0, accentL = 0;
    RGBToHLS(accent.get(), accentH, accentS, accentL);
    CColor light = accent;
    bool foundSecond = false;
    float bestDist = 0.f;
    for (const auto &bk : ranked) {
        CColor c = colorFromBucket(bk);
        float h = 0, s = 0, l = 0;
        RGBToHLS(c.get(), h, s, l);
        const float dist = hueDistanceDeg(h, accentH);
        if (dist >= 50.f && dist > bestDist && s > 0.12f) {
            bestDist = dist;
            light = c;
            foundSecond = true;
        }
    }
    {
        float h = 0, s = 0, l = 0;
        if (foundSecond) {
            RGBToHLS(light.get(), h, s, l);
        } else {
            h = std::fmod(accentH + 72.f, 360.f);
            s = std::max(0.4f, accentS * 0.85f);
            l = 0.62f;
        }
        light.set(HLSToRGB(h, std::clamp(s + 0.1f, 0.3f, 0.95f),
                            std::clamp(std::max(l, 0.55f) * 0.55f + 0.4f, 0.55f, 0.88f)));
    }

    m_palette[0] = dark;
    m_palette[1] = mid;
    m_palette[2] = accent;
    m_palette[3] = light;
    for (int i = 0; i < kPaletteSize; ++i) {
        m_palette[i].setAlpha(255);
    }
}

void CAlbumArtVisualizerCtrl::updateDisplayLevels(const AudioAnalysisFrame &frame, int bandCount) {
    if ((int)m_displayLevels.size() != bandCount) {
        m_displayLevels.assign(bandCount, 0.f);
    }
    if (frame.sessionId != m_lastSessionId) {
        m_lastSessionId = frame.sessionId;
        std::fill(m_displayLevels.begin(), m_displayLevels.end(), 0.f);
        m_ringSmooth = 0.f;
        m_lowEnergySmooth = 0.f;
        m_lowOnsetFlash = 0.f;
        m_lowOnsetStrengthSmooth = 0.f;
        m_bassPulseFlash = 0.f;
        m_bassStrengthSmooth = 0.f;
        m_beatPulseFlash = 0.f;
        m_beatStrengthSmooth = 0.f;
        m_onsetStrengthSmooth = 0.f;
        m_bassRhythmRateSmooth = 0.f;
        m_rhythmConfidenceSmooth = 0.f;
    }

    for (int i = 0; i < bandCount; ++i) {
        float level = 0.f;
        if (frame.bandCount > 0) {
            const float srcPos = (float)i * (float)frame.bandCount / (float)bandCount;
            const int i0 = std::min((int)frame.bandCount - 1, (int)srcPos);
            const int i1 = std::min((int)frame.bandCount - 1, i0 + 1);
            const float t = srcPos - (float)i0;
            level = frame.bands[i0] * (1.f - t) + frame.bands[i1] * t;
        }
        level = std::clamp(level, 0.f, 1.f);

        float &disp = m_displayLevels[i];
        if (level > disp) {
            disp = disp + (level - disp) * kAttack;
        } else {
            disp = disp + (level - disp) * kRelease;
        }
    }
}

void CAlbumArtVisualizerCtrl::updateReactiveSmooth(const AudioAnalysisFrame &frame, bool hasFrame) {
    const float rms = hasFrame ? frame.rms : 0.f;
    const float lowEnergy =
        hasFrame ? (frame.lowEnergy > 0.001f ? frame.lowEnergy : frame.bass) : 0.f;
    const float lowOnset = hasFrame ? frame.lowOnset : 0.f;
    const float lowOnsetStrength = hasFrame ? frame.lowOnsetStrength : 0.f;
    const float bassPulse = hasFrame ? frame.bassPulse : 0.f;
    const float bassStrength = hasFrame ? frame.bassStrength : 0.f;
    const float beatPulse =
        hasFrame ? (frame.beatPulse > 0.001f ? frame.beatPulse : frame.beat) : 0.f;
    const float beatStrength = hasFrame ? frame.beatStrength : 0.f;
    const float onsetStrength = hasFrame ? frame.onsetStrength : 0.f;
    const float bassRhythmRate = hasFrame ? frame.bassRhythmRate : 0.f;
    const float rhythmConfidence = hasFrame ? frame.rhythmConfidence : 0.f;

    m_lowEnergySmooth += (lowEnergy - m_lowEnergySmooth) * 0.18f;

    const float targetRing = 0.35f + m_lowEnergySmooth * 0.55f + rms * 0.15f;
    m_ringSmooth += (targetRing - m_ringSmooth) * 0.22f;

    auto followPulse = [](float &smooth, float raw) {
        smooth = std::max(raw, smooth * 0.82f);
        if (raw < smooth) {
            smooth += (raw - smooth) * 0.35f;
        }
    };
    followPulse(m_lowOnsetFlash, lowOnset);
    followPulse(m_bassPulseFlash, bassPulse);
    followPulse(m_beatPulseFlash, beatPulse);

    m_lowOnsetStrengthSmooth +=
        (lowOnsetStrength - m_lowOnsetStrengthSmooth) *
        (lowOnsetStrength > m_lowOnsetStrengthSmooth ? 0.45f : 0.2f);
    m_bassStrengthSmooth = std::max(bassStrength, m_bassStrengthSmooth * 0.9f);
    if (bassStrength < m_bassStrengthSmooth) {
        m_bassStrengthSmooth += (bassStrength - m_bassStrengthSmooth) * 0.2f;
    }
    m_beatStrengthSmooth = std::max(beatStrength, m_beatStrengthSmooth * 0.9f);
    if (beatStrength < m_beatStrengthSmooth) {
        m_beatStrengthSmooth += (beatStrength - m_beatStrengthSmooth) * 0.2f;
    }
    m_onsetStrengthSmooth += (onsetStrength - m_onsetStrengthSmooth) *
                             (onsetStrength > m_onsetStrengthSmooth ? 0.45f : 0.2f);
    m_bassRhythmRateSmooth += (bassRhythmRate - m_bassRhythmRateSmooth) * 0.08f;
    m_rhythmConfidenceSmooth += (rhythmConfidence - m_rhythmConfidenceSmooth) * 0.1f;
}

void CAlbumArtVisualizerCtrl::draw(CRawGraph *canvas) {
    CUIObject::draw(canvas);

    const CRect rc = m_rcObj;
    if (rc.width() <= 4 || rc.height() <= 4) {
        return;
    }

    if (!m_effect) {
        ensureEffect(m_effectId.c_str());
    }

    AudioAnalysisFrame frame;
    bool hasFrame = false;
    {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        hasFrame = m_hasFrame;
        if (hasFrame) {
            frame = m_frame;
        }
    }

    const int bandCount = std::clamp(m_bandCount, 24, 96);
    updateDisplayLevels(hasFrame ? frame : AudioAnalysisFrame{}, bandCount);
    updateReactiveSmooth(frame, hasFrame);

    const float dt = consumeDtSec();
    AlbumArtVizContext ctx = buildContext(canvas, hasFrame ? &frame : nullptr, hasFrame);

    if (m_transition && !m_transition->isFinished()) {
        m_transition->update(dt);
        m_transition->draw(ctx);
        if (m_transition->isFinished()) {
            finishTransition();
        }
        return;
    }

    if (m_effect) {
        m_effect->update(ctx, dt);
        m_effect->draw(ctx);
    }
}

bool CAlbumArtVisualizerCtrl::setProperty(cstr_t szProperty, cstr_t szValue) {
    if (CUIObject::setProperty(szProperty, szValue)) {
        return true;
    }

    if (strcasecmp(szProperty, SZ_PN_IMAGE) == 0) {
        m_strImgFile = szValue;
    } else if (strcasecmp(szProperty, "ArtRatio") == 0) {
        m_artRatio = std::clamp((float)atof(szValue), 0.25f, 0.78f);
    } else if (strcasecmp(szProperty, "BandCount") == 0) {
        m_bandCount = std::clamp(atoi(szValue), 24, 96);
    } else if (strcasecmp(szProperty, "Palette") == 0) {
        if (!parsePalette(szValue)) {
            return false;
        }
    } else if (strcasecmp(szProperty, "FallbackColor") == 0) {
        CColor accent;
        getColorValue(accent, szValue);
        if (!m_paletteFromXml) {
            applyFallbackColor(accent);
        } else {
            m_fallbackColor = accent;
            m_fallbackColor.setAlpha(255);
            m_defaultPalette[2] = m_fallbackColor;
            if (!m_hasAlbumArt) {
                resetDefaultPalette();
            }
        }
    } else if (strcasecmp(szProperty, "Effect") == 0) {
        m_effectId = szValue ? szValue : defaultAlbumArtEffectId();
        if (m_pSkin) {
            requestEffect(m_effectId.c_str());
        }
    } else if (strcasecmp(szProperty, "Transition") == 0) {
        m_transitionId = szValue ? szValue : defaultAlbumArtTransitionId();
    } else if (strcasecmp(szProperty, "RotateMode") == 0) {
        m_rotateMode = szValue ? szValue : "bars";
        forwardRotateModeToEffect();
        saveRotateMode();
    } else {
        return false;
    }
    return true;
}
