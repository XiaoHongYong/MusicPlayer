#include "DebugMetersEffect.h"
#include "../AlbumArtVizDrawUtil.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

CColor rgba(int r, int g, int b, int a = 255) {
    return CColor(RGB(r, g, b), (uint8_t)std::clamp(a, 0, 255));
}

CColor meterColor(float t) {
    // 绿 → 黄 → 红
    const float u = std::clamp(t, 0.f, 1.f);
    if (u < 0.5f) {
        const float k = u * 2.f;
        return rgba((int)(40 + k * 200), (int)(180 + k * 40), 60, 230);
    }
    const float k = (u - 0.5f) * 2.f;
    return rgba(240, (int)(220 - k * 160), 50, 230);
}

} // namespace

void DebugMetersEffect::ensureFont(float scaleFactor) {
    const float s = scaleFactor > 0.1f ? scaleFactor : 1.f;
    if (m_fontScale > 0.f && std::fabs(m_fontScale - s) < 0.01f && m_font.getFontInfo().isValid()) {
        return;
    }
    m_fontScale = s;
    const int px = std::max(10, (int)std::lround(11.f * s));
#if defined(_MAC_OS)
    m_font.create(FontInfoEx("Menlo", "PingFang SC", px, FW_NORMAL, 0, false), s);
#elif defined(_WIN32)
    m_font.create(FontInfoEx("Consolas", "Microsoft YaHei", px, FW_NORMAL, 0, false), 1.f);
#else
    m_font.create(FontInfoEx("DejaVu Sans Mono", "Noto Sans CJK SC", px, FW_NORMAL, 0, false), 1.f);
#endif
}

void DebugMetersEffect::update(const AlbumArtVizContext &ctx, float /*dtSec*/) {
    const AudioAnalysisFrame *f = (ctx.hasFrame && ctx.frame) ? ctx.frame : nullptr;
    const float bass = f ? f->bass : 0.f;
    const float mid = f ? f->mid : 0.f;
    const float treble = f ? f->treble : 0.f;
    const float rms = f ? f->rms : 0.f;
    // 顺序须与 draw() 中 meters[] 一致（原始帧）
    const float values[] = {
        f ? f->lowEnergy : ctx.lowEnergy,
        f ? f->lowOnset : ctx.lowOnset,
        f ? f->bassPulse : ctx.bassPulse,
        f ? f->bassStrength : ctx.bassStrength,
        f ? f->beatPulse : ctx.beatPulse,
        f ? f->beatStrength : ctx.beatStrength,
        f ? f->lowOnsetStrength : ctx.lowOnsetStrength,
        f ? f->onsetStrength : ctx.onsetStrength,
        bass,
        mid,
        treble,
        std::clamp(rms * 2.f, 0.f, 1.f),
    };
    const int n = (int)(sizeof(values) / sizeof(values[0]));
    for (int i = 0; i < n && i < 16; ++i) {
        if (values[i] > m_peakHold[i]) {
            m_peakHold[i] = values[i];
        } else {
            m_peakHold[i] = std::max(0.f, m_peakHold[i] - 0.012f);
        }
    }
}

void DebugMetersEffect::drawMeter(CRawGraph *canvas, const CRect &barRc, const Meter &m,
                                  float scale) const {
    if (!canvas || barRc.width() < 2 || barRc.height() < 4) {
        return;
    }

    // 底槽
    canvas->fillRect(barRc, rgba(28, 30, 36, 200), BPM_BLEND);

    const float h = (float)barRc.height();
    const float fillH = std::max(1.f, h * std::clamp(m.value, 0.f, 1.f));
    CRect fill = barRc;
    fill.top = (int)std::lround((float)barRc.bottom - fillH);
    canvas->fillRect(fill, m.color, BPM_BLEND);

    // 峰值线
    // （峰值由 update 维护，这里通过 display 旁路：若 value 接近 peak 则更亮）
    char num[48];
    std::snprintf(num, sizeof(num), "%.2f", m.display);

    canvas->setTextColor(rgba(220, 224, 230));

    const int labelH = std::max(12, (int)std::lround(13.f * scale));
    CRect rcLabel(barRc.left - 2, barRc.bottom + 2, barRc.right + 2, barRc.bottom + 2 + labelH * 2);
    canvas->drawText(m.label, (int)strlen(m.label), rcLabel, DT_CENTER | DT_TOP | DT_SINGLELINE);

    canvas->setTextColor(rgba(255, 230, 120));
    CRect rcVal(barRc.left - 4, barRc.top - labelH - 2, barRc.right + 4, barRc.top - 1);
    canvas->drawText(num, (int)strlen(num), rcVal, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);
}

void DebugMetersEffect::draw(const AlbumArtVizContext &ctx) {
    if (!ctx.canvas) {
        return;
    }
    const CRect &rc = ctx.bounds;
    if (rc.width() <= 8 || rc.height() <= 8) {
        return;
    }

    ensureFont(ctx.scaleFactor);
    CRawGraph *canvas = ctx.canvas;
    if (m_font.getFontInfo().isValid()) {
        canvas->setFont(&m_font);
    }

    // 暗底，便于读数
    canvas->fillRect(rc, rgba(12, 14, 18, 230), BPM_BLEND);

    // 角落小封面作参照
    const float minDim = (float)std::min(rc.width(), rc.height());
    const float artHalf = minDim * 0.14f;
    const float artCx = (float)rc.left + artHalf + 10.f * ctx.scaleFactor;
    const float artCy = (float)rc.top + artHalf + 10.f * ctx.scaleFactor;
    albumart_viz::drawCircularAlbumArt(ctx, artCx, artCy, artHalf, 0.f);

    // 调试优先看分析帧原始值（未经宿主二次门槛），宿主平滑值只作对照
    const AudioAnalysisFrame *f = (ctx.hasFrame && ctx.frame) ? ctx.frame : nullptr;
    const float lowE = f ? f->lowEnergy : ctx.lowEnergy;
    const float lowOn = f ? f->lowOnset : ctx.lowOnset;
    const float bPulse = f ? f->bassPulse : ctx.bassPulse;
    const float bStr = f ? f->bassStrength : ctx.bassStrength;
    const float pulse = f ? f->beatPulse : ctx.beatPulse;
    const float btStr = f ? f->beatStrength : ctx.beatStrength;
    const float lowOs = f ? f->lowOnsetStrength : ctx.lowOnsetStrength;
    const float onset = f ? f->onsetStrength : ctx.onsetStrength;
    const float bass = f ? f->bass : 0.f;
    const float mid = f ? f->mid : 0.f;
    const float treble = f ? f->treble : 0.f;
    const float rms = f ? f->rms : 0.f;

    const Meter meters[] = {
        {"lowE", lowE, lowE, meterColor(lowE)},
        {"lowOn", lowOn, lowOn, meterColor(lowOn)},
        {"bPuls", bPulse, bPulse, meterColor(bPulse)},
        {"bStr", bStr, bStr, meterColor(bStr)},
        {"pulse", pulse, pulse, meterColor(pulse)},
        {"btStr", btStr, btStr, meterColor(btStr)},
        {"loStr", lowOs, lowOs, meterColor(lowOs)},
        {"onset", onset, onset, meterColor(onset)},
        {"bass", bass, bass, meterColor(bass)},
        {"mid", mid, mid, meterColor(mid)},
        {"treb", treble, treble, meterColor(treble)},
        {"rms", std::clamp(rms * 2.f, 0.f, 1.f), rms, meterColor(std::clamp(rms * 2.f, 0.f, 1.f))},
    };
    const int count = (int)(sizeof(meters) / sizeof(meters[0]));

    const float scale = ctx.scaleFactor > 0.1f ? ctx.scaleFactor : 1.f;
    const int pad = (int)std::lround(12.f * scale);
    const int topPad = (int)std::lround(36.f * scale);
    const int bottomPad = (int)std::lround(36.f * scale);
    const int gap = (int)std::lround(6.f * scale);

    const int areaL = rc.left + pad;
    const int areaR = rc.right - pad;
    const int areaT = rc.top + topPad;
    const int areaB = rc.bottom - bottomPad;
    const int areaW = std::max(1, areaR - areaL);
    const int areaH = std::max(1, areaB - areaT);

    const float barW = std::max(8.f, ((float)areaW - gap * (count - 1)) / (float)count);
    const int barH = areaH;

    // 标题
    {
        char title[128];
        std::snprintf(title, sizeof(title), "debug-meters (raw frame)  %s  seq=%llu",
                      ctx.hasFrame ? "ok" : "—",
                      ctx.hasFrame && ctx.frame ? (unsigned long long)ctx.frame->sequence : 0ull);
        canvas->setTextColor(rgba(180, 190, 210));
        CRect rcTitle(areaL, rc.top + (int)std::lround(6.f * scale), areaR,
                      rc.top + topPad - 4);
        canvas->drawText(title, (int)strlen(title), rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // 0.5 / 1.0 参考线
    {
        m_pen.createSolidPen(1, rgba(60, 65, 75, 180));
        canvas->setPen(m_pen);
        const int y50 = areaB - (int)std::lround(barH * 0.5f);
        const int y100 = areaT;
        canvas->line((float)areaL, (float)y50, (float)areaR, (float)y50);
        canvas->line((float)areaL, (float)y100, (float)areaR, (float)y100);
    }

    for (int i = 0; i < count; ++i) {
        const int x0 = areaL + (int)std::lround(i * (barW + gap));
        const int x1 = (int)std::lround((float)x0 + barW);
        CRect barRc(x0, areaT, x1, areaB);

        Meter m = meters[i];
        // 叠加 peak hold 细线
        const float peak = (i < 16) ? m_peakHold[i] : 0.f;
        drawMeter(canvas, barRc, m, scale);

        if (peak > 0.02f) {
            const int yPeak = areaB - (int)std::lround((float)barH * std::clamp(peak, 0.f, 1.f));
            m_pen.createSolidPen(std::max(1, (int)std::lround(2.f * scale)),
                                 rgba(255, 255, 255, 200));
            canvas->setPen(m_pen);
            canvas->line((float)x0, (float)yPeak, (float)x1, (float)yPeak);
        }
    }

    // 图例说明
    {
        canvas->setTextColor(rgba(140, 148, 160));
        const char *hint =
            "lowE/lowOn/bPuls=Bass  pulse/btStr=通用打击  loStr=连续低频起音  onset=Flux";
        CRect rcHint(areaL, rc.bottom - bottomPad + (int)std::lround(4.f * scale), areaR,
                     rc.bottom - (int)std::lround(4.f * scale));
        canvas->drawText(hint, (int)strlen(hint), rcHint, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}
