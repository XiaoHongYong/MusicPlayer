#pragma once

#include "../MPlayerEngine/IPlayerCore.hpp"
#include "../Skin/UIObject.h"
#include "CurMediaAlbumArt.h"
#include "albumart-viz/AlbumArtEffectFactory.h"
#include "albumart-viz/AlbumArtVizContext.h"
#include "albumart-viz/IAlbumArtVizHost.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

/**
 * 封面可视化宿主控件。
 * XML 标签名：AlbumArtVisualizer
 *
 * 负责：音频订阅、封面/取色、平滑、特效调度与转场。
 * 具体画面由 IAlbumArtEffect 绘制；特效切换走 IAlbumArtEffectTransition。
 *
 * 单击：循环切换特效（跳过 debug）。
 * 双击：切换到 debug-meters。
 *
 * 属性：
 *   Image          无封面时的占位图
 *   ArtRatio       封面直径占控件短边的比例（默认 0.64）
 *   BandCount      径向频谱条数（默认 64，最大 96）
 *   Palette        无封面时的 4 阶色板，逗号分隔 dark,mid,accent,light
 *   FallbackColor  无封面主色（便捷项：据此派生 Palette；有 Palette 时以 Palette 为准）
 *   Effect         特效 id：sonic-halo / plain-cover / living-cover / audio-reactor / debug-meters
 *   Transition     转场 id（默认 crossfade）
 */
class CAlbumArtVisualizerCtrl : public CUIObject,
                                public IAudioAnalysisSink,
                                public IEventHandler,
                                public IAlbumArtVizHost {
    UIOBJECT_CLASS_NAME_DECLARE(CUIObject)
public:
    CAlbumArtVisualizerCtrl();
    ~CAlbumArtVisualizerCtrl() override;

    void onCreate() override;
    void onSize() override;
    void onTimer(int nId) override;
    void draw(CRawGraph *canvas) override;
    bool setProperty(cstr_t szProperty, cstr_t szValue) override;
    bool onLButtonDown(uint32_t nFlags, CPoint point) override;
    bool onLButtonClick(uint32_t nFlags, CPoint point) override;
    bool onLButtonDblClk(uint32_t nFlags, CPoint point) override;

    void onEvent(const IEvent *pEvent) override;
    void onAudioAnalysisFrame(const AudioAnalysisFrame &frame) override;

    void invalidateViz() override { invalidate(); }

protected:
    void scheduleInvalidate();
    void syncAnalysisSubscription();
    void updateAlbumArt();
    void rebuildAlbumArtImage(const RawImageDataPtr &src);
    void extractPalette(const RawImageDataPtr &src);
    void sampleRayColors(const RawImageDataPtr &src);
    void resetDefaultPalette();
    void applyFallbackColor(const CColor &accent);
    bool parsePalette(cstr_t szValue);
    static CColor boostForViz(const CColor &c, float satBoost = 0.35f);

    void updateDisplayLevels(const AudioAnalysisFrame &frame, int bandCount);
    void updateReactiveSmooth(const AudioAnalysisFrame &frame, bool hasFrame);

    void ensureEffect(cstr_t id);
    void requestEffect(cstr_t id);
    void cycleEffect();
    void finishTransition();
    void notifyEffectsAlbumArtChanged();
    void beginArtCrossfade();
    void finishArtCrossfade();
    void updateArtCrossfade(float dtSec);
    bool isArtCrossfading() const {
        return m_artCrossfadeProgress < 1.f &&
               (m_imgArtPrev.isValid() || m_imgArtOrgPrev.isValid());
    }
    AlbumArtVizContext buildContext(CRawGraph *canvas, const AudioAnalysisFrame *frame,
                                    bool hasFrame);
    float consumeDtSec();

    void loadEffectId();
    void saveEffectId() const;

    std::mutex m_frameMutex;
    AudioAnalysisFrame m_frame;
    bool m_hasFrame = false;
    uint64_t m_lastSessionId = 0;

    std::vector<float> m_displayLevels;
    float m_ringSmooth = 0.f;
    float m_lowEnergySmooth = 0.f;
    float m_lowOnsetFlash = 0.f;
    float m_lowOnsetStrengthSmooth = 0.f;
    float m_bassPulseFlash = 0.f;
    float m_bassStrengthSmooth = 0.f;
    float m_beatPulseFlash = 0.f;
    float m_beatStrengthSmooth = 0.f;
    float m_onsetStrengthSmooth = 0.f;
    float m_bassRhythmRateSmooth = 0.f;
    float m_rhythmConfidenceSmooth = 0.f;

    CCurMediaAlbumArt m_mediaAlbumArt;
    string m_strImgFile;
    CSFImage m_imgArt;
    CSFImage m_imgArtOrg;
    CSFImage m_imgArtPrev; // 多封面切换交叉淡入时的上一张（圆形）
    CSFImage m_imgArtOrgPrev; // 交叉淡入时的上一张原图
    float m_artCrossfadeProgress = 1.f; // 0→1；>=1 表示无转场
    bool m_hasAlbumArt = false;

    static constexpr int kPaletteSize = 4;
    static constexpr int kRayColorCount = 96;
    static constexpr int kParticlePaletteSize = 5; // 与 AudioReactor 固定粒子数对齐
    CColor m_palette[kPaletteSize];
    CColor m_defaultPalette[kPaletteSize];
    CColor m_rayColors[kRayColorCount];
    CColor m_particleColors[kParticlePaletteSize];
    CColor m_fallbackColor;
    bool m_paletteFromXml = false;

    float m_artRatio = 0.64f;
    int m_bandCount = 64;
    string m_effectId;
    string m_transitionId;

    std::unique_ptr<IAlbumArtEffect> m_effect;
    std::unique_ptr<IAlbumArtEffect> m_nextEffect;
    std::unique_ptr<IAlbumArtEffectTransition> m_transition;

    bool m_subscribedToAnalysis = false;
    std::shared_ptr<std::atomic<bool>> m_alive;
    std::atomic<bool> m_invalidatePending{false};

    std::chrono::steady_clock::time_point m_lastDrawTime{};
    bool m_hasLastDrawTime = false;
};
