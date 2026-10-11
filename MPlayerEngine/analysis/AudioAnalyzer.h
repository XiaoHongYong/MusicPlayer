#pragma once

#include "AudioAnalysisFrame.h"
#include "AudioAnalysisOptions.h"
#include "BassRhythmEstimator.h"
#include "Fft.h"
#include "LowOnsetDetector.h"
#include "SpectralFluxOnsetDetector.h"

#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * 播放端实时分析流水线（平台无关）：
 *
 *   PCM → mono → ring
 *     → 每个 hopSize 做 FFT / Onset / 节奏（分析频率）
 *     → 按 outputRate 决定是否发布 AudioAnalysisFrame（输出频率）
 *
 * 二者分离：降低输出帧率不会跳过鼓点检测。
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
    void resetDetectors();
    void resetPublishSchedule();

    /** 处理一个分析窗口；若应发布则填充 out 并返回 true。始终消耗 hop（数据足够时）。 */
    bool processHop(AudioAnalysisFrame &out);
    bool shouldPublish(uint64_t center);

    void buildHann();
    void rebuildBandTables();
    void computeMagnitude();
    void mapLogBands(float *bandsOut, uint16_t bandCount) const;
    void applyFloorRelative(float *bands, uint16_t bandCount, float deltaSeconds);
    void smoothBands(float *bands, uint16_t bandCount, float deltaSeconds);
    void updateRhythmFeatures(AudioAnalysisFrame &out, float deltaSeconds);

    AudioAnalysisOptions _options;
    Fft _fft;
    LowOnsetDetector _lowOnset;
    SpectralFluxOnsetDetector _spectralFlux;
    BassRhythmEstimator _bassRhythm;

    uint64_t _sessionId = 0;
    uint64_t _sequence = 0;
    uint64_t _samplePos = 0; // ring 中最早样点的绝对位置（窗口起点）
    uint32_t _sampleRate = 0;

    std::vector<float> _ring;
    size_t _ringCap = 0;
    size_t _ringWrite = 0;
    size_t _ringCount = 0;

    std::vector<float> _hann;
    std::vector<float> _time;
    std::vector<float> _complex;
    std::vector<float> _mag;

    std::vector<uint16_t> _bandK0;
    std::vector<uint16_t> _bandK1;

    std::vector<float> _smooth;
    std::vector<float> _floor;

    uint64_t _nextPublishSample = 0;
    bool _publishScheduleReady = false;

    bool _smoothReady = false;

    float _beatPulseEnv = 0.f;
    float _beatStrengthHold = 0.f;
    float _lowOnsetEnv = 0.f;
    float _bassPulseEnv = 0.f;
    float _bassStrengthHold = 0.f;

    static constexpr float kPulseDecaySec = 0.12f;
    static constexpr float kStrengthDecaySec = 0.45f;
    static constexpr float kBandAttackSec = 0.025f;
    static constexpr float kBandReleaseSec = 0.12f;
    static constexpr float kFloorUpSec = 0.55f;
    static constexpr float kFloorDownSec = 0.18f;
    static constexpr float kFMinHz = 20.f;
    static constexpr float kFMaxHz = 18000.f;
    static constexpr float kDbMin = -58.f;
    static constexpr float kDbMax = 0.f;
    static constexpr float kGamma = 0.82f;
    static constexpr float kFloorMix = 0.45f;
    static constexpr float kDisplayGain = 1.4f;
    static constexpr float kLowShelfEnd = 0.20f;
    static constexpr float kLowShelfMid = 0.10f;
    static constexpr float kLowShelfGain0 = 0.80f;
    static constexpr float kLowShelfGainMid = 0.80f;
};
