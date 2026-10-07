#include "MPlayerApp.h"
#include "AudioVisualizerCtrl.h"
#include "../MPlayer/Player.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

UIOBJECT_CLASS_NAME_IMP(CAudioVisualizerCtrl, "AudioVisualizer")

namespace {
constexpr float kPeakFall = 0.02f;
constexpr float kAttack = 0.75f;
constexpr float kRelease = 0.35f;
constexpr cstr_t kProfileKey = "AudioVisualizerMode";
} // namespace

CAudioVisualizerCtrl::CAudioVisualizerCtrl() {
    m_msgNeed = UO_MSG_WANT_LBUTTON;
    m_alive = std::make_shared<std::atomic<bool>>(true);
    m_barColor.set(RGB(46, 230, 200));
    m_peakColor.set(RGB(232, 255, 249));
    m_bgColor.set(RGB(0, 0, 0));
    m_bgColor.setAlpha(0);
    m_pen.createSolidPen(2, m_barColor);
}

CAudioVisualizerCtrl::~CAudioVisualizerCtrl() {
    *m_alive = false;
    if (m_subscribedToAnalysis) {
        g_player.unsubscribeAudioAnalysis(this);
        m_subscribedToAnalysis = false;
    }
}

CAudioVisualizerCtrl::Mode CAudioVisualizerCtrl::modeFromString(cstr_t s) {
    if (!s) {
        return Mode::SpectrumBars;
    }
    if (strcasecmp(s, "none") == 0 || strcasecmp(s, "off") == 0) {
        return Mode::None;
    }
    if (strcasecmp(s, "waveform") == 0) {
        return Mode::Waveform;
    }
    if (strcasecmp(s, "spectrum-circle") == 0 || strcasecmp(s, "circle") == 0) {
        return Mode::Circle;
    }
    return Mode::SpectrumBars;
}

cstr_t CAudioVisualizerCtrl::modeToString(Mode mode) {
    switch (mode) {
    case Mode::Waveform:
        return "waveform";
    case Mode::Circle:
        return "spectrum-circle";
    case Mode::None:
        return "none";
    case Mode::SpectrumBars:
    default:
        return "spectrum-bars";
    }
}

void CAudioVisualizerCtrl::loadMode() {
    m_mode = modeFromString(g_profile.getString(SZ_SECT_UI, kProfileKey, modeToString(m_mode)));
}

void CAudioVisualizerCtrl::saveMode() const {
    g_profile.writeString(SZ_SECT_UI, kProfileKey, modeToString(m_mode));
}

void CAudioVisualizerCtrl::syncAnalysisSubscription() {
    const bool want = m_mode != Mode::None;
    if (want == m_subscribedToAnalysis) {
        return;
    }
    if (want) {
        AudioAnalysisOptions opt;
        opt.bandCount = 64;
        opt.outputRate = 30;
        opt.fftSize = 2048;
        opt.hopSize = 512;
        g_player.setAudioAnalysisOptions(opt);
        g_player.subscribeAudioAnalysis(this);
        m_subscribedToAnalysis = true;
    } else {
        g_player.unsubscribeAudioAnalysis(this);
        m_subscribedToAnalysis = false;
        std::lock_guard<std::mutex> lock(m_frameMutex);
        m_hasFrame = false;
        m_displayLevels.clear();
        m_peakLevels.clear();
        m_ringSmooth = 0.f;
    }
}

void CAudioVisualizerCtrl::cycleMode() {
    switch (m_mode) {
    case Mode::SpectrumBars:
        m_mode = Mode::Waveform;
        break;
    case Mode::Waveform:
        m_mode = Mode::Circle;
        break;
    case Mode::Circle:
        m_mode = Mode::None;
        break;
    case Mode::None:
    default:
        m_mode = Mode::SpectrumBars;
        break;
    }
    saveMode();
    syncAnalysisSubscription();
    invalidate();
}

void CAudioVisualizerCtrl::onCreate() {
    CUIObject::onCreate();
    loadMode();
    syncAnalysisSubscription();
}

void CAudioVisualizerCtrl::onAudioAnalysisFrame(const AudioAnalysisFrame &frame) {
    if (m_mode == Mode::None) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        m_frame = frame;
        m_hasFrame = true;
    }
    scheduleInvalidate();
}

void CAudioVisualizerCtrl::scheduleInvalidate() {
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

bool CAudioVisualizerCtrl::onLButtonUp(uint32_t nFlags, CPoint point) {
    if (!m_rcObj.ptInRect(point)) {
        return false;
    }
    cycleMode();
    return true;
}

int CAudioVisualizerCtrl::calcSpectrumColumns(int width) const {
    const int gap = std::max(0, m_gap);
    const int minW = std::max(1, m_minBarWidth);
    const int slot = minW + gap;
    if (width <= 0 || slot <= 0) {
        return 8;
    }
    int count = (width + gap) / slot;
    count = std::max(8, count);
    count = std::min(count, (int)AudioAnalysisFrame::kMaxBands);
    return count;
}

void CAudioVisualizerCtrl::updateDisplayLevels(const AudioAnalysisFrame &frame, int bandCount) {
    if ((int)m_displayLevels.size() != bandCount) {
        m_displayLevels.assign(bandCount, 0.f);
        m_peakLevels.assign(bandCount, 0.f);
    }
    if (frame.sessionId != m_lastSessionId) {
        m_lastSessionId = frame.sessionId;
        std::fill(m_displayLevels.begin(), m_displayLevels.end(), 0.f);
        std::fill(m_peakLevels.begin(), m_peakLevels.end(), 0.f);
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

        float &peak = m_peakLevels[i];
        if (disp > peak) {
            peak = disp;
        } else {
            peak = std::max(0.f, peak - kPeakFall);
        }
    }
}

void CAudioVisualizerCtrl::drawSpectrumBars(CRawGraph *canvas, const CRect &rc, int bandCount) {
    const int gap = std::max(0, m_gap);
    const int totalGap = gap * (bandCount - 1);
    const int barArea = std::max(bandCount, rc.width() - totalGap);
    const float barW = (float)barArea / (float)bandCount;
    const int height = rc.height();
    const int peakH = std::max(1, height / 24);

    for (int i = 0; i < bandCount; ++i) {
        const int x0 = rc.left + (int)std::lround(i * (barW + gap));
        const int x1 = rc.left + (int)std::lround(i * (barW + gap) + barW);
        const int w = std::max(1, x1 - x0);

        int barH = (int)std::lround(m_displayLevels[i] * (float)height);
        if (barH > 0) {
            barH = std::max(m_minBarHeight, barH);
            const int y0 = rc.bottom - barH;
            canvas->fillRect(CRect(x0, y0, x0 + w, rc.bottom), m_barColor, BPM_BLEND);
        }

        const int peakY = rc.bottom - (int)std::lround(m_peakLevels[i] * (float)height) - peakH;
        if (m_peakLevels[i] > 0.02f && peakY < rc.bottom - 1) {
            canvas->fillRect(
                CRect(x0, std::max(rc.top, peakY), x0 + w, std::max(rc.top, peakY) + peakH),
                m_peakColor, BPM_BLEND);
        }
    }
}

void CAudioVisualizerCtrl::drawWaveform(CRawGraph *canvas, const CRect &rc,
                                        const AudioAnalysisFrame &frame, bool hasFrame) {
    const int n = std::max(32, calcSpectrumColumns(rc.width()) * 2);
    if ((int)m_displayLevels.size() != n) {
        m_displayLevels.assign(n, 0.f);
        m_peakLevels.clear();
    }

    for (int i = 0; i < n; ++i) {
        float tgt = 0.f;
        if (hasFrame && frame.bandCount > 0) {
            const float t = (float)i / (float)(n - 1);
            const int bi = std::min((int)frame.bandCount - 1, (int)(t * frame.bandCount));
            const float mag = frame.bands[bi];
            const float phase = std::sin(t * (float)M_PI * 6.f + (float)frame.sequence * 0.15f);
            tgt = mag * phase * (0.55f + frame.rms);
        }
        float &cur = m_displayLevels[i];
        if (std::fabs(tgt) > std::fabs(cur)) {
            cur = cur + (tgt - cur) * 0.5f;
        } else {
            cur = cur + (tgt - cur) * 0.25f;
        }
    }

    const float mid = (float)(rc.top + rc.bottom) * 0.5f;
    const float amp = (float)rc.height() * 0.42f;

    CColor fill = m_barColor;
    fill.setAlpha(64);
    VecPoints poly;
    poly.reserve((size_t)n * 2 + 2);
    for (int i = 0; i < n; ++i) {
        const float x = (float)rc.left + (float)i / (float)(n - 1) * (float)rc.width();
        const float y = mid - m_displayLevels[i] * amp;
        poly.push_back(CPoint((int)std::lround(x), (int)std::lround(y)));
    }
    for (int i = n - 1; i >= 0; --i) {
        const float x = (float)rc.left + (float)i / (float)(n - 1) * (float)rc.width();
        const float y = mid + m_displayLevels[i] * amp * 0.85f;
        poly.push_back(CPoint((int)std::lround(x), (int)std::lround(y)));
    }
    canvas->fillPath(poly, fill);

    m_pen.createSolidPen(2, m_barColor);
    canvas->setPen(m_pen);
    for (int i = 1; i < n; ++i) {
        const float x0 = (float)rc.left + (float)(i - 1) / (float)(n - 1) * (float)rc.width();
        const float x1 = (float)rc.left + (float)i / (float)(n - 1) * (float)rc.width();
        const float y0 = mid - m_displayLevels[i - 1] * amp;
        const float y1 = mid - m_displayLevels[i] * amp;
        canvas->line(x0, y0, x1, y1);
    }
}

void CAudioVisualizerCtrl::drawCircle(CRawGraph *canvas, const CRect &rc,
                                      const AudioAnalysisFrame &frame, bool hasFrame) {
    const int bandCount = std::min(48, calcSpectrumColumns(rc.width()));
    updateDisplayLevels(hasFrame ? frame : AudioAnalysisFrame{}, bandCount);

    const float bass = hasFrame ? frame.bass : 0.f;
    const float rms = hasFrame ? frame.rms : 0.f;
    const float targetRing = 0.35f + bass * 0.45f + rms * 0.2f;
    m_ringSmooth += (targetRing - m_ringSmooth) * 0.2f;

    const float cx = (float)(rc.left + rc.right) * 0.5f;
    const float cy = (float)(rc.top + rc.bottom) * 0.5f;
    const float minDim = (float)std::min(rc.width(), rc.height());
    const float baseR = minDim * 0.22f * (0.92f + m_ringSmooth * 0.18f);
    const float maxLen = minDim * 0.28f;
    const float spin = hasFrame ? (float)frame.sequence * 0.02f : 0.f;

    CColor ringClr = m_peakColor;
    ringClr.setAlpha(140);
    m_pen.createSolidPen(std::max(1, (int)std::lround(2.f + bass * 4.f)), ringClr);
    canvas->setPen(m_pen);
    // 近似圆环：多段折线
    const int ringSeg = 48;
    for (int i = 0; i < ringSeg; ++i) {
        const float a0 = (float)(2.0 * M_PI * i / ringSeg);
        const float a1 = (float)(2.0 * M_PI * (i + 1) / ringSeg);
        const float r = baseR * 0.72f;
        canvas->line(cx + std::cos(a0) * r, cy + std::sin(a0) * r,
                     cx + std::cos(a1) * r, cy + std::sin(a1) * r);
    }

    for (int i = 0; i < bandCount; ++i) {
        const float a0 = spin + (float)i / (float)bandCount * (float)(2.0 * M_PI);
        const float a1 = spin + ((float)i + 0.7f) / (float)bandCount * (float)(2.0 * M_PI);
        const float len = std::max(2.f, m_displayLevels[i] * maxLen);
        VecPoints quad;
        quad.push_back(CPoint((int)std::lround(cx + std::cos(a0) * baseR),
                              (int)std::lround(cy + std::sin(a0) * baseR)));
        quad.push_back(CPoint((int)std::lround(cx + std::cos(a0) * (baseR + len)),
                              (int)std::lround(cy + std::sin(a0) * (baseR + len))));
        quad.push_back(CPoint((int)std::lround(cx + std::cos(a1) * (baseR + len)),
                              (int)std::lround(cy + std::sin(a1) * (baseR + len))));
        quad.push_back(CPoint((int)std::lround(cx + std::cos(a1) * baseR),
                              (int)std::lround(cy + std::sin(a1) * baseR)));
        canvas->fillPath(quad, m_barColor);
    }
}

void CAudioVisualizerCtrl::draw(CRawGraph *canvas) {
    CUIObject::draw(canvas);

    const CRect rc = m_rcObj;
    if (rc.width() <= 0 || rc.height() <= 0) {
        return;
    }

    if (m_bgColor.getAlpha() > 0) {
        canvas->fillRect(rc, m_bgColor, BPM_BLEND);
    }

    if (m_mode == Mode::None) {
        return;
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

    if (m_mode == Mode::Waveform) {
        drawWaveform(canvas, rc, frame, hasFrame);
        return;
    }
    if (m_mode == Mode::Circle) {
        drawCircle(canvas, rc, frame, hasFrame);
        return;
    }

    const int bandCount = calcSpectrumColumns(rc.width());
    if (hasFrame) {
        updateDisplayLevels(frame, bandCount);
    } else {
        updateDisplayLevels(AudioAnalysisFrame{}, bandCount);
    }
    drawSpectrumBars(canvas, rc, bandCount);
}

bool CAudioVisualizerCtrl::setProperty(cstr_t szProperty, cstr_t szValue) {
    if (CUIObject::setProperty(szProperty, szValue)) {
        return true;
    }

    if (strcasecmp(szProperty, "BarColor") == 0) {
        getColorValue(m_barColor, szValue);
        m_pen.createSolidPen(2, m_barColor);
    } else if (strcasecmp(szProperty, "PeakColor") == 0) {
        getColorValue(m_peakColor, szValue);
    } else if (strcasecmp(szProperty, "BgColor") == 0) {
        getColorValue(m_bgColor, szValue);
    } else if (strcasecmp(szProperty, "Gap") == 0) {
        m_gap = atoi(szValue);
    } else if (strcasecmp(szProperty, "MinBarHeight") == 0) {
        m_minBarHeight = atoi(szValue);
    } else if (strcasecmp(szProperty, "MinBarWidth") == 0) {
        m_minBarWidth = std::max(1, atoi(szValue));
    } else if (strcasecmp(szProperty, "Mode") == 0) {
        m_mode = modeFromString(szValue);
        if (m_bCreated) {
            syncAnalysisSubscription();
        }
    } else if (strcasecmp(szProperty, "BandCount") == 0) {
        // 兼容旧皮肤：忽略，列数按宽度自动计算
    } else {
        return false;
    }
    return true;
}
