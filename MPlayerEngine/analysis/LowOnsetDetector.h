#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

/**
 * 低频起音：固定 dB 映射的 lowEnergy + 相对慢基线的 dB 起音。
 * hit 用阈值上穿；静音/极弱低频不触发（minLowEnergyDb）。
 */
struct LowOnsetResult {
    float lowEnergy = 0.f; // 固定 dB 范围 → 0..1
    float novelty = 0.f;   // dB 上升量（无有效低频时为 0）
    float onset = 0.f;     // 连续起音强度 0..1
    bool hit = false;
};

class LowOnsetDetector {
public:
    struct Options {
        float lowHz = 40.f;
        float highHz = 200.f;
        float energySmoothSeconds = 0.35f; // 慢基线上升时间常数
        float baselineSeconds = 1.5f;
        float thresholdMultiplier = 1.35f;
        float minThreshold = 3.f;        // 最低起音阈值，单位 dB
        float minLowEnergyDb = -45.f;    // 低于此绝对能量不认冲击
        float refractorySeconds = 0.10f;
        // lowEnergy 固定 dB 映射（需按 FFT 标定校准）
        float lowDbFloor = -55.f;
        float lowDbCeil = -6.f;
    };

    LowOnsetDetector() = default;
    explicit LowOnsetDetector(Options options) : _options(options) {}

    void reset() {
        _initialized = false;
        _energySlow = 0.f;
        _mean = 0.f;
        _variance = 0.f;
        _refractoryRemaining = 0.f;
        _wasAboveThreshold = false;
    }

    LowOnsetResult process(const float *magnitude, size_t binCount, uint32_t sampleRate,
                           size_t fftSize, float deltaSeconds) {
        LowOnsetResult result;
        if (!magnitude || binCount == 0 || sampleRate == 0 || fftSize == 0 || deltaSeconds <= 0.f) {
            return result;
        }

        const float binHz = (float)sampleRate / (float)fftSize;
        const size_t firstBin =
            std::max<size_t>(1, (size_t)std::ceil(_options.lowHz / std::max(1e-6f, binHz)));
        const size_t lastBin =
            std::min(binCount - 1, (size_t)std::floor(_options.highHz / std::max(1e-6f, binHz)));
        if (firstBin > lastBin) {
            return result;
        }

        double power = 0.0;
        for (size_t k = firstBin; k <= lastBin; ++k) {
            const double m = std::max(0.f, magnitude[k]);
            power += m * m;
        }
        const float energy = (float)power;

        constexpr float kPowerFloor = 1e-12f;
        const float energyDb = 10.f * std::log10(std::max(energy, kPowerFloor));
        const float dbSpan = std::max(1.f, _options.lowDbCeil - _options.lowDbFloor);
        result.lowEnergy =
            std::clamp((energyDb - _options.lowDbFloor) / dbSpan, 0.f, 1.f);

        if (!_initialized) {
            _initialized = true;
            _energySlow = energy;
            _refractoryRemaining = 0.f;
            _wasAboveThreshold = false;
            return result;
        }

        // 先用上一时刻慢基线算起音，再更新基线（避免本帧冲击被提前学进参考）
        const float slowDb = 10.f * std::log10(std::max(_energySlow, kPowerFloor));
        const float noveltyDb = std::max(0.f, energyDb - slowDb);
        const bool hasEnoughLowEnergy = energyDb >= _options.minLowEnergyDb;
        result.novelty = hasEnoughLowEnergy ? noveltyDb : 0.f;

        const float stddev = std::sqrt(std::max(0.f, _variance));
        const float thresholdDb =
            std::max(_options.minThreshold, _mean + _options.thresholdMultiplier * stddev);

        // 约 12 dB 上升对应满量程
        result.onset =
            hasEnoughLowEnergy ? std::clamp(noveltyDb / 12.f, 0.f, 1.f) : 0.f;

        _refractoryRemaining = std::max(0.f, _refractoryRemaining - deltaSeconds);

        const bool aboveThreshold = hasEnoughLowEnergy && noveltyDb >= thresholdDb;
        const bool crossedThreshold = aboveThreshold && !_wasAboveThreshold;
        if (crossedThreshold && _refractoryRemaining <= 0.f) {
            result.hit = true;
            result.onset =
                std::clamp(0.35f + (noveltyDb - thresholdDb) / 12.f, 0.f, 1.f);
            _refractoryRemaining = _options.refractorySeconds;
        }
        _wasAboveThreshold = aboveThreshold;

        if (!result.hit && !aboveThreshold) {
            const float alpha =
                1.f - std::exp(-deltaSeconds / std::max(0.1f, _options.baselineSeconds));
            const float oldMean = _mean;
            const float n = hasEnoughLowEnergy ? noveltyDb : 0.f;
            _mean += alpha * (n - _mean);
            _variance += alpha * ((n - oldMean) * (n - _mean) - _variance);
        }

        const float riseAlpha =
            1.f - std::exp(-deltaSeconds / std::max(0.05f, _options.energySmoothSeconds));
        const float fallAlpha = 1.f - std::exp(-deltaSeconds / 0.10f);
        if (energy > _energySlow) {
            _energySlow += (energy - _energySlow) * riseAlpha;
        } else {
            _energySlow += (energy - _energySlow) * fallAlpha;
        }

        return result;
    }

private:
    Options _options;
    bool _initialized = false;
    float _energySlow = 0.f;
    float _mean = 0.f;
    float _variance = 0.f;
    float _refractoryRemaining = 0.f;
    bool _wasAboveThreshold = false;
};
