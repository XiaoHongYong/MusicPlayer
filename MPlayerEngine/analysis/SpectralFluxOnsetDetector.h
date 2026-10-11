#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * 节拍向 Spectral Flux（80–3000Hz）。
 * onsetStrength 连续；hit 用延迟峰值确认。
 * 驱动 beatPulse 的打击事件（非完整 Beat Tracking）。
 *
 * Flux 为正向 log 幅度变化的求和（不除以 bin 数）。
 * 阈值 / pulse 按该求和尺度标定。
 */
struct SpectralFluxResult {
    float onsetStrength = 0.f;
    float pulse = 0.f;
    bool hit = false;
};

class SpectralFluxOnsetDetector {
public:
    struct Options {
        float lowHz = 80.f;
        float highHz = 3000.f;
        float baselineSeconds = 2.2f;
        float thresholdMultiplier = 1.6f;
        // 实测背景 Mean≈0.5–0.7；Candidate≈2.0–2.5 多为有效打击，勿再抬到 1.5+
        float minThreshold = 1.2f;
        float overshoot = 1.15f;
        float refractorySeconds = 0.16f;
        float fluxRefAttackSec = 0.12f;
        float fluxRefReleaseSec = 2.0f;
        // 按 Uptown Funk 日志：cand≈2.2 → pulse≈0.35，应过门槛
        float minPulse = 0.18f;
        // pulse = (cand - thr) / (thr + pulseHeadroom)
        float pulseHeadroom = 1.2f;
    };

    SpectralFluxOnsetDetector() = default;
    explicit SpectralFluxOnsetDetector(Options options) : _options(options) {}

    void reset() {
        _prevLog.clear();
        _initialized = false;
        _mean = 0.f;
        _variance = 0.f;
        _refractoryRemaining = 0.f;
        _fluxRef = 0.f;
        _prevFluxN = 0.f;
        _candFlux = 0.f;
        _hasCandidate = false;
    }

    SpectralFluxResult process(const float *magnitude, size_t binCount, uint32_t sampleRate,
                               size_t fftSize, float deltaSeconds) {
        SpectralFluxResult result;
        if (!magnitude || binCount < 2 || sampleRate == 0 || fftSize == 0 || deltaSeconds <= 0.f) {
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

        if (_prevLog.size() != binCount) {
            _prevLog.assign(binCount, 0.f);
            for (size_t k = 0; k < binCount; ++k) {
                _prevLog[k] = std::log1p(std::max(0.f, magnitude[k]) * 8.f);
            }
            _initialized = false;
            return result;
        }

        // 正向频谱变化求和（不除以 bin 数）
        double flux = 0.0;
        for (size_t k = firstBin; k <= lastBin; ++k) {
            const float lg = std::log1p(std::max(0.f, magnitude[k]) * 8.f);
            const float d = lg - _prevLog[k];
            if (d > 0.f) {
                flux += (double)d;
            }
            _prevLog[k] = lg;
        }
        for (size_t k = 1; k < firstBin; ++k) {
            _prevLog[k] = std::log1p(std::max(0.f, magnitude[k]) * 8.f);
        }
        for (size_t k = lastBin + 1; k < binCount; ++k) {
            _prevLog[k] = std::log1p(std::max(0.f, magnitude[k]) * 8.f);
        }
        _prevLog[0] = std::log1p(std::max(0.f, magnitude[0]) * 8.f);

        const float fluxN = (float)flux;

        if (_fluxRef <= 1e-8f) {
            _fluxRef = std::max(fluxN, 1e-5f);
        } else {
            const float tau =
                fluxN > _fluxRef ? _options.fluxRefAttackSec : _options.fluxRefReleaseSec;
            const float alpha = 1.f - std::exp(-deltaSeconds / std::max(0.001f, tau));
            _fluxRef += (fluxN - _fluxRef) * alpha;
        }
        result.onsetStrength = std::clamp(fluxN / std::max(_fluxRef * 1.15f, 1e-5f), 0.f, 1.f);

        if (!_initialized) {
            _initialized = true;
            _prevFluxN = fluxN;
            // 用首帧播种基线，避免 Mean≈0 导致 Thr 长期卡在 minThreshold
            _mean = fluxN;
            _variance = 0.f;
            return result;
        }

        const float stddev = std::sqrt(std::max(0.f, _variance));
        const float thr =
            std::max(_options.minThreshold, _mean + _options.thresholdMultiplier * stddev);

        _refractoryRemaining = std::max(0.f, _refractoryRemaining - deltaSeconds);

        if (_hasCandidate && fluxN < _candFlux && _refractoryRemaining <= 0.f) {
            const float headroom = std::max(0.5f, _options.pulseHeadroom);
            const float pulse =
                std::clamp((_candFlux - thr) / (thr + headroom), 0.f, 1.f);
            if (_candFlux >= thr * _options.overshoot && pulse >= _options.minPulse) {
                result.hit = true;
                result.pulse = pulse;
                _refractoryRemaining = _options.refractorySeconds;
            }
            _hasCandidate = false;
        }

        if (fluxN >= thr * _options.overshoot && fluxN >= _prevFluxN &&
            _refractoryRemaining <= 0.f) {
            _candFlux = fluxN;
            _hasCandidate = true;
        } else if (fluxN < _prevFluxN) {
            if (_hasCandidate && _candFlux < thr * _options.overshoot) {
                _hasCandidate = false;
            }
        }

        _prevFluxN = fluxN;

        // 背景学习：非 hit、无候选时，只学低于当前阈值的帧（求和尺度下 thr*0.9 会几乎永不满足）
        if (!result.hit && !_hasCandidate && fluxN < thr) {
            const float bAlpha =
                1.f - std::exp(-deltaSeconds / std::max(0.1f, _options.baselineSeconds));
            const float oldMean = _mean;
            _mean += bAlpha * (fluxN - _mean);
            _variance += bAlpha * ((fluxN - oldMean) * (fluxN - _mean) - _variance);
        }

        return result;
    }

private:
    Options _options;
    std::vector<float> _prevLog;
    bool _initialized = false;
    float _mean = 0.f;
    float _variance = 0.f;
    float _refractoryRemaining = 0.f;
    float _fluxRef = 0.f;
    float _prevFluxN = 0.f;
    float _candFlux = 0.f;
    bool _hasCandidate = false;
};
