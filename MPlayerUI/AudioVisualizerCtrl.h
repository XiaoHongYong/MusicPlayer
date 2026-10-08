#pragma once

#include "../MPlayerEngine/IPlayerCore.hpp"
#include "../Skin/UIObject.h"
#include "../GfxRaw/RawPen.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/**
 * 桌面音频可视化控件：消费 AudioAnalysisFrame。
 * XML 标签名：AudioVisualizer
 *
 * 点击在 Modes 允许的特效间循环切换（默认：spectrum-bars → waveform →
 * spectrum-circle → none）。模式写入 g_profile [MusicPlayer] AudioVisualizerMode；
 * none 时不绘制，并取消分析订阅以省 CPU。
 *
 * 属性：
 *   BarColor / PeakColor / BgColor / Gap / MinBarHeight / MinBarWidth
 *   Mode（可选初始模式，会被已保存设置覆盖；若不在 Modes 内则钳到首个允许项）
 *   Modes（逗号分隔允许列表，如 "spectrum-bars,waveform"；省略则全部可切换）
 */
class CAudioVisualizerCtrl : public CUIObject, public IAudioAnalysisSink {
    UIOBJECT_CLASS_NAME_DECLARE(CUIObject)
public:
    enum class Mode {
        SpectrumBars,
        Waveform,
        Circle,
        None,
    };

    CAudioVisualizerCtrl();
    ~CAudioVisualizerCtrl() override;

    void onCreate() override;
    void draw(CRawGraph *canvas) override;
    bool setProperty(cstr_t szProperty, cstr_t szValue) override;
    bool onLButtonUp(uint32_t nFlags, CPoint point) override;

    void onAudioAnalysisFrame(const AudioAnalysisFrame &frame) override;

protected:
    void scheduleInvalidate();
    void loadMode();
    void saveMode() const;
    void cycleMode();
    void syncAnalysisSubscription();
    void parseAllowedModes(cstr_t szValue);
    bool isModeAllowed(Mode mode) const;
    Mode clampToAllowed(Mode mode) const;
    static Mode modeFromString(cstr_t s);
    static cstr_t modeToString(Mode mode);

    int calcSpectrumColumns(int width) const;
    void updateDisplayLevels(const AudioAnalysisFrame &frame, int bandCount);
    void drawSpectrumBars(CRawGraph *canvas, const CRect &rc, int bandCount);
    void drawWaveform(CRawGraph *canvas, const CRect &rc, const AudioAnalysisFrame &frame, bool hasFrame);
    void drawCircle(CRawGraph *canvas, const CRect &rc, const AudioAnalysisFrame &frame, bool hasFrame);

    std::mutex m_frameMutex;
    AudioAnalysisFrame m_frame;
    bool m_hasFrame = false;
    uint64_t m_lastSessionId = 0;
    std::vector<float> m_displayLevels;
    std::vector<float> m_peakLevels;
    float m_ringSmooth = 0.f;

    Mode m_mode = Mode::SpectrumBars;
    // 空 = 默认全开；非空时点击只在列表内循环
    std::vector<Mode> m_allowedModes;
    bool m_subscribedToAnalysis = false;
    CColor m_barColor;
    CColor m_peakColor;
    CColor m_bgColor;
    CRawPen m_pen;
    int m_gap = 2;
    int m_minBarHeight = 1;
    int m_minBarWidth = 3;

    std::shared_ptr<std::atomic<bool>> m_alive;
    std::atomic<bool> m_invalidatePending{false};
};
