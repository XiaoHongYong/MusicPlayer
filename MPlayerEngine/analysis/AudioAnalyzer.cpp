#include "AudioAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

uint16_t nextPow2AtLeast(uint16_t v, uint16_t minV = 64, uint16_t maxV = 8192) {
    uint16_t n = minV;
    while (n < v && n < maxV) {
        n = (uint16_t)(n << 1);
    }
    return n;
}

} // namespace

AudioAnalyzer::AudioAnalyzer() {
    applyOptions(AudioAnalysisOptions{});
}

void AudioAnalyzer::setOptions(const AudioAnalysisOptions &options) {
    applyOptions(options);
}

void AudioAnalyzer::applyOptions(const AudioAnalysisOptions &options) {
    AudioAnalysisOptions o = options;
    o.fftSize = nextPow2AtLeast(o.fftSize == 0 ? 2048 : o.fftSize);
    if (o.hopSize == 0 || o.hopSize > o.fftSize) {
        o.hopSize = o.fftSize / 2;
    }
    if (o.bandCount == 0) {
        o.bandCount = 64;
    }
    o.bandCount = (uint16_t)std::min<size_t>(o.bandCount, AudioAnalysisFrame::kMaxBands);
    if (o.outputRate == 0) {
        o.outputRate = 30;
    }

    _options = o;
    _fft.configure(_options.fftSize);

    const size_t n = _options.fftSize;
    _ringCap = n * 4;
    _ring.assign(_ringCap, 0.f);
    _ringWrite = 0;
    _ringCount = 0;

    _hann.resize(n);
    _time.resize(n);
    _complex.resize(n * 2);
    _mag.resize(n / 2);
    _smooth.assign(_options.bandCount, 0.f);
    _floor.assign(_options.bandCount, 0.f);
    _smoothReady = false;

    buildHann();
    rebuildBandTables();
}

void AudioAnalyzer::buildHann() {
    const size_t n = _hann.size();
    if (n < 2) {
        return;
    }
    for (size_t i = 0; i < n; ++i) {
        _hann[i] = 0.5f * (1.f - std::cos((float)(2.0 * M_PI * (double)i / (double)(n - 1))));
    }
}

void AudioAnalyzer::rebuildBandTables() {
    const uint16_t bandCount = _options.bandCount;
    _bandK0.assign(bandCount, 0);
    _bandK1.assign(bandCount, 0);
    if (bandCount == 0 || _options.fftSize < 4 || _sampleRate == 0) {
        return;
    }

    const size_t n = _options.fftSize;
    const size_t half = n / 2;
    const float nyquist = (float)_sampleRate * 0.5f;
    const float fMin = kFMinHz;
    const float fMax = std::min(kFMaxHz, nyquist);
    if (fMax <= fMin * 1.5f) {
        return;
    }

    auto hzToBin = [&](float hz) -> size_t {
        const double b = (double)hz * (double)n / (double)_sampleRate;
        return (size_t)std::lround(b);
    };

    size_t kMin = std::max<size_t>(1, hzToBin(fMin));
    size_t kMaxEx = std::min(half, hzToBin(fMax) + 1); // exclusive
    if (kMaxEx <= kMin + 1) {
        return;
    }

    // 对数边界 → bin，再强制每带至少 1 个独立 bin（避免空带回退到同一 nearest）
    std::vector<size_t> edges(bandCount + 1);
    const float logMin = std::log(fMin);
    const float logMax = std::log(fMax);
    edges[0] = kMin;
    edges[bandCount] = kMaxEx;
    for (uint16_t i = 1; i < bandCount; ++i) {
        const float t = (float)i / (float)bandCount;
        const float f = std::exp(logMin + (logMax - logMin) * t);
        edges[i] = hzToBin(f);
    }

    for (uint16_t i = 1; i < bandCount; ++i) {
        if (edges[i] <= edges[i - 1]) {
            edges[i] = edges[i - 1] + 1;
        }
    }
    for (int i = (int)bandCount - 1; i >= 0; --i) {
        if (edges[(size_t)i + 1] <= edges[(size_t)i]) {
            if (edges[(size_t)i + 1] > 0) {
                edges[(size_t)i] = edges[(size_t)i + 1] - 1;
            }
        }
    }
    edges[0] = std::max(edges[0], kMin);
    edges[bandCount] = std::min(edges[bandCount], kMaxEx);
    for (uint16_t i = 1; i < bandCount; ++i) {
        if (edges[i] <= edges[i - 1]) {
            edges[i] = edges[i - 1] + 1;
        }
        if (edges[i] >= edges[bandCount]) {
            edges[i] = edges[bandCount] > i ? edges[bandCount] - ((size_t)bandCount - i) : edges[i - 1];
        }
    }

    for (uint16_t b = 0; b < bandCount; ++b) {
        size_t k0 = edges[b];
        size_t k1 = edges[b + 1];
        if (k1 <= k0) {
            k1 = k0; // 空带：输出 0，绝不借用邻带
        }
        k0 = std::min(k0, half);
        k1 = std::min(k1, half);
        _bandK0[b] = (uint16_t)k0;
        _bandK1[b] = (uint16_t)k1;
    }
}

void AudioAnalyzer::clearRing() {
    _ringWrite = 0;
    _ringCount = 0;
    if (!_ring.empty()) {
        std::fill(_ring.begin(), _ring.end(), 0.f);
    }
}

void AudioAnalyzer::reset(uint64_t sessionId, uint64_t samplePosition) {
    _sessionId = sessionId;
    _sequence = 0;
    _samplePos = samplePosition;
    _lastEmitCenter = samplePosition;
    clearRing();
    _smoothReady = false;
    std::fill(_smooth.begin(), _smooth.end(), 0.f);
    std::fill(_floor.begin(), _floor.end(), 0.f);
}

void AudioAnalyzer::seek(uint64_t samplePosition) {
    _samplePos = samplePosition;
    _lastEmitCenter = samplePosition;
    clearRing();
    _smoothReady = false;
    std::fill(_smooth.begin(), _smooth.end(), 0.f);
    std::fill(_floor.begin(), _floor.end(), 0.f);
}

void AudioAnalyzer::pushMono(float sample) {
    if (_ringCap == 0) {
        return;
    }
    _ring[_ringWrite] = sample;
    _ringWrite = (_ringWrite + 1) % _ringCap;
    if (_ringCount < _ringCap) {
        ++_ringCount;
    } else {
        ++_samplePos;
    }
}

void AudioAnalyzer::computeMagnitude() {
    const size_t n = _options.fftSize;
    const size_t half = n / 2;
    const float scale = 2.8f / (float)n;

    for (size_t k = 0; k < half; ++k) {
        const float re = _complex[k * 2];
        const float im = _complex[k * 2 + 1];
        _mag[k] = std::sqrt(re * re + im * im) * scale;
    }
}

void AudioAnalyzer::mapLogBands(float *bandsOut, uint16_t bandCount) const {
    for (uint16_t b = 0; b < bandCount; ++b) {
        bandsOut[b] = 0.f;
    }
    if (_bandK0.size() != bandCount || _mag.empty()) {
        return;
    }

    const size_t half = _mag.size();
    for (uint16_t b = 0; b < bandCount; ++b) {
        const size_t k0 = _bandK0[b];
        const size_t k1 = _bandK1[b];
        if (k1 <= k0 || k0 >= half) {
            continue;
        }
        const size_t end = std::min(k1, half);

        float sumPower = 0.f;
        float weight = 0.f;
        for (size_t k = k0; k < end; ++k) {
            const float m = _mag[k];
            sumPower += m * m;
            weight += 1.f;
        }
        if (weight <= 0.f) {
            continue;
        }

        const float bandMag = std::sqrt(sumPower / weight);
        const float db = 20.f * std::log10(std::max(bandMag, 1e-12f));
        const float lin = std::clamp((db - kDbMin) / (kDbMax - kDbMin), 0.f, 1.f);
        float level = std::pow(lin, kGamma);

        // 压制最低约 20% 频带，避免 20Hz 起的 sub 轻易顶满
        if (bandCount > 1) {
            const float t = (float)b / (float)(bandCount - 1);
            float shelf = 1.f;
            if (t < kLowShelfMid) {
                const float u = t / kLowShelfMid;
                shelf = kLowShelfGain0 + (kLowShelfGainMid - kLowShelfGain0) * u;
            } else if (t < kLowShelfEnd) {
                const float u = (t - kLowShelfMid) / (kLowShelfEnd - kLowShelfMid);
                shelf = kLowShelfGainMid + (1.f - kLowShelfGainMid) * u;
            }
            level *= shelf;
        }
        bandsOut[b] = std::clamp(level, 0.f, 1.f);
    }
}

void AudioAnalyzer::applyFloorRelative(float *bands, uint16_t bandCount) {
    if (_floor.size() < bandCount) {
        _floor.assign(bandCount, 0.f);
    }
    for (uint16_t i = 0; i < bandCount; ++i) {
        float &fl = _floor[i];
        const float x = bands[i];
        const float a = x > fl ? kFloorUp : kFloorDown;
        fl += (x - fl) * a;

        // 扣掉部分持续底床，再统一抬显示增益
        const float denom = std::max(1e-4f, 1.f - kFloorMix * fl);
        const float rel = std::clamp((x - kFloorMix * fl) / denom, 0.f, 1.f);
        bands[i] = std::clamp(rel * kDisplayGain, 0.f, 1.f);
    }
}

void AudioAnalyzer::smoothBands(float *bands, uint16_t bandCount) {
    if (!_smoothReady) {
        for (uint16_t i = 0; i < bandCount; ++i) {
            _smooth[i] = bands[i];
        }
        _smoothReady = true;
        return;
    }
    for (uint16_t i = 0; i < bandCount; ++i) {
        const float cur = _smooth[i];
        const float tgt = bands[i];
        const float a = tgt > cur ? kAttack : kRelease;
        _smooth[i] = cur + (tgt - cur) * a;
        bands[i] = _smooth[i];
    }
}

bool AudioAnalyzer::tryEmit(AudioAnalysisFrame &out) {
    const size_t n = _options.fftSize;
    const size_t hop = _options.hopSize;
    if (_ringCount < n || _sampleRate == 0) {
        return false;
    }

    const uint64_t center = _samplePos + n / 2;
    if (_options.outputRate > 0 && _sequence > 0) {
        const uint64_t minStep = std::max<uint64_t>(1, (uint64_t)_sampleRate / _options.outputRate);
        if (center < _lastEmitCenter + minStep) {
            const size_t drop = std::min(hop, _ringCount);
            _ringCount -= drop;
            _samplePos += drop;
            return false;
        }
    }

    const size_t start = (_ringWrite + _ringCap - _ringCount) % _ringCap;
    float rmsAcc = 0.f;
    float peak = 0.f;
    for (size_t i = 0; i < n; ++i) {
        const float s = _ring[(start + i) % _ringCap];
        _time[i] = s * _hann[i];
        rmsAcc += s * s;
        const float a = std::fabs(s);
        if (a > peak) {
            peak = a;
        }
    }

    _fft.forward(_time.data(), _complex.data());
    computeMagnitude();

    out = AudioAnalysisFrame{};
    out.sessionId = _sessionId;
    out.sequence = ++_sequence;
    out.samplePosition = _samplePos;
    out.sampleRate = _sampleRate;
    out.rms = std::sqrt(rmsAcc / (float)n);
    out.peak = peak;
    out.beat = 0.f;
    out.bandCount = _options.bandCount;

    mapLogBands(out.bands.data(), out.bandCount);
    applyFloorRelative(out.bands.data(), out.bandCount);
    smoothBands(out.bands.data(), out.bandCount);

    const uint16_t nb = out.bandCount;
    const uint16_t b1 = std::max<uint16_t>(1, nb / 6);
    const uint16_t b2 = std::max<uint16_t>((uint16_t)(b1 + 1), (uint16_t)(nb * 2 / 3));
    auto avg = [&](uint16_t from, uint16_t to) {
        float s = 0.f;
        uint16_t c = 0;
        for (uint16_t i = from; i < to && i < nb; ++i) {
            s += out.bands[i];
            ++c;
        }
        return c ? s / (float)c : 0.f;
    };
    out.bass = avg(0, b1);
    out.mid = avg(b1, b2);
    out.treble = avg(b2, nb);

    _lastEmitCenter = center;

    const size_t drop = std::min(hop, _ringCount);
    _ringCount -= drop;
    _samplePos += drop;
    return true;
}

size_t AudioAnalyzer::pushInterleavedS16(const int16_t *samples, size_t frameCount, int channels,
                                         uint32_t sampleRate, AudioAnalysisFrame *outFrames,
                                         size_t maxOut) {
    if (!samples || !outFrames || frameCount == 0 || channels <= 0 || sampleRate == 0 || maxOut == 0) {
        return 0;
    }

    if (_sampleRate != sampleRate) {
        clearRing();
        _smoothReady = false;
        std::fill(_smooth.begin(), _smooth.end(), 0.f);
        std::fill(_floor.begin(), _floor.end(), 0.f);
        _sampleRate = sampleRate;
        rebuildBandTables();
    }

    if (channels == 1) {
        for (size_t i = 0; i < frameCount; ++i) {
            pushMono(samples[i] / 32768.f);
        }
    } else {
        for (size_t i = 0; i < frameCount; ++i) {
            float sum = 0.f;
            for (int c = 0; c < channels; ++c) {
                sum += samples[i * (size_t)channels + (size_t)c] / 32768.f;
            }
            pushMono(sum / (float)channels);
        }
    }

    size_t produced = 0;
    while (produced < maxOut && _ringCount >= _options.fftSize) {
        const size_t countBefore = _ringCount;
        AudioAnalysisFrame frame;
        if (tryEmit(frame)) {
            outFrames[produced++] = frame;
        } else if (_ringCount >= countBefore) {
            break;
        }
    }
    return produced;
}
