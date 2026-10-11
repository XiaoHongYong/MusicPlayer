#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

/**
 * 由低频起音时间戳估计 bassRhythmRate（Hz）与 rhythmConfidence。
 * 更新较慢：仅在 hit 时推进，每帧可读当前估计。
 */
class BassRhythmEstimator {
public:
    static constexpr size_t kMaxIntervals = 8;

    struct Options {
        float minIntervalSec = 0.18f; // ~333 BPM 上限保护
        float maxIntervalSec = 2.0f;  // ~30 BPM 下限
        float holdSeconds = 2.5f;     // 无新 hit 时估计衰减时间
    };

    BassRhythmEstimator() = default;
    explicit BassRhythmEstimator(Options options) : _options(options) {}

    void reset() {
        _count = 0;
        _write = 0;
        _hasLast = false;
        _lastHitSec = 0.0;
        _rate = 0.f;
        _confidence = 0.f;
        _ageSec = 0.f;
    }

    void onLowHit(double timeSec) {
        bool acceptedInterval = false;
        if (_hasLast) {
            const float interval = (float)(timeSec - _lastHitSec);
            if (interval >= _options.minIntervalSec && interval <= _options.maxIntervalSec) {
                _intervals[_write] = interval;
                _write = (_write + 1) % kMaxIntervals;
                if (_count < kMaxIntervals) {
                    ++_count;
                }
                recompute();
                acceptedInterval = true;
            }
        }
        _lastHitSec = timeSec;
        _hasLast = true;
        // 只有有效间隔才刷新估计存活时间，避免假阳性拖住旧 confidence
        if (acceptedInterval) {
            _ageSec = 0.f;
        }
    }

    void tick(float deltaSeconds) {
        if (deltaSeconds <= 0.f) {
            return;
        }
        _ageSec += deltaSeconds;
        if (_ageSec > _options.holdSeconds && _confidence > 0.f) {
            const float fade =
                std::exp(-deltaSeconds / std::max(0.3f, _options.holdSeconds * 0.5f));
            _confidence *= fade;
            if (_confidence < 0.05f) {
                _confidence = 0.f;
                _rate = 0.f;
                _count = 0;
                _write = 0;
            }
        }
    }

    float rateHz() const { return _rate; }
    float confidence() const { return _confidence; }

private:
    void recompute() {
        if (_count < 2) {
            _rate = _count == 1 ? 1.f / std::max(1e-3f, _intervals[0]) : 0.f;
            _confidence = _count == 1 ? 0.25f : 0.f;
            return;
        }

        float tmp[kMaxIntervals];
        for (size_t i = 0; i < _count; ++i) {
            tmp[i] = _intervals[i];
        }
        std::sort(tmp, tmp + _count);
        const float median = tmp[_count / 2];
        _rate = 1.f / std::max(1e-3f, median);

        float mean = 0.f;
        for (size_t i = 0; i < _count; ++i) {
            mean += tmp[i];
        }
        mean /= (float)_count;
        float var = 0.f;
        for (size_t i = 0; i < _count; ++i) {
            const float d = tmp[i] - mean;
            var += d * d;
        }
        var /= (float)_count;
        const float cv = std::sqrt(var) / std::max(1e-3f, mean);
        // 间隔越整齐置信度越高；样本越多略抬升
        const float regularity = std::clamp(1.f - cv * 1.8f, 0.f, 1.f);
        const float sampleFactor = std::clamp((_count - 1) / 5.f, 0.2f, 1.f);
        _confidence = regularity * sampleFactor;
    }

    Options _options;
    float _intervals[kMaxIntervals]{};
    size_t _count = 0;
    size_t _write = 0;
    bool _hasLast = false;
    double _lastHitSec = 0.0;
    float _rate = 0.f;
    float _confidence = 0.f;
    float _ageSec = 0.f;
};
