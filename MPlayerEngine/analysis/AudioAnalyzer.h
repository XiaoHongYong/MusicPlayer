#pragma once

#include "AudioAnalysisFrame.h"
#include "AudioAnalysisOptions.h"
#include "Fft.h"

#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * 播放端实时分析流水线（平台无关）：
 *
 *   PCM (int16 interleaved)
 *     → mono downmix
 *     → ring buffer
 *     → 取 fftSize 样点
 *     → Hann 窗
 *     → FFT
 *     → |X[k]| (跳过 DC)
 *     → 对数频带 RMS（bin 对齐，每带至少 1 bin）
 *     → dB → 0..1 → gamma(<1 抬弱信号)
 *     → 慢速底噪跟随 + 显示增益
 *     → attack/release 平滑
 *     → AudioAnalysisFrame
 */
class AudioAnalyzer {
public:
    AudioAnalyzer();

    void setOptions(const AudioAnalysisOptions &options);
    const AudioAnalysisOptions &options() const { return _options; }

    void reset(uint64_t sessionId, uint64_t samplePosition = 0);
    void seek(uint64_t samplePosition);

    uint64_t sessionId() const { return _sessionId; }

    size_t pushInterleavedS16(const int16_t *samples, size_t frameCount, int channels,
                              uint32_t sampleRate, AudioAnalysisFrame *outFrames, size_t maxOut);

private:
    void applyOptions(const AudioAnalysisOptions &options);
    void clearRing();
    void pushMono(float sample);
    bool tryEmit(AudioAnalysisFrame &out);

    void buildHann();
    void rebuildBandTables();
    void computeMagnitude();
    void mapLogBands(float *bandsOut, uint16_t bandCount) const;
    void applyFloorRelative(float *bands, uint16_t bandCount);
    void smoothBands(float *bands, uint16_t bandCount);

    AudioAnalysisOptions _options;
    Fft _fft;

    uint64_t _sessionId = 0;
    uint64_t _sequence = 0;
    uint64_t _samplePos = 0;
    uint32_t _sampleRate = 0;

    std::vector<float> _ring;
    size_t _ringCap = 0;
    size_t _ringWrite = 0;
    size_t _ringCount = 0;

    std::vector<float> _hann;
    std::vector<float> _time;
    std::vector<float> _complex;
    std::vector<float> _mag;

    // 每带 FFT bin 半开区间 [k0, k1)
    std::vector<uint16_t> _bandK0;
    std::vector<uint16_t> _bandK1;

    std::vector<float> _smooth;
    std::vector<float> _floor; // 慢速底噪，压持续低频床

    uint64_t _lastEmitCenter = 0;
    bool _smoothReady = false;

    static constexpr float kFMinHz = 20.f;
    static constexpr float kFMaxHz = 18000.f;
    static constexpr float kDbMin = -58.f;
    static constexpr float kDbMax = 0.f;
    // gamma < 1：全频段抬低幅度响应，弱信号更灵敏，强信号仍不易顶满
    static constexpr float kGamma = 0.82f;
    static constexpr float kAttack = 0.9f;
    static constexpr float kRelease = 0.6f;
    // 底噪跟随：保留节奏起伏，但少扣一点，避免整体柱高过矮
    static constexpr float kFloorUp = 0.018f;
    static constexpr float kFloorDown = 0.06f;
    static constexpr float kFloorMix = 0.45f;
    static constexpr float kDisplayGain = 1.4f;
    // 最低 20% 频带压制：最底端约 ×0.42，10% 处 ×0.70，20% 以上恢复 1.0
    static constexpr float kLowShelfEnd = 0.20f;
    static constexpr float kLowShelfMid = 0.10f;
    static constexpr float kLowShelfGain0 = 0.80f;
    static constexpr float kLowShelfGainMid = 0.80f;
};
