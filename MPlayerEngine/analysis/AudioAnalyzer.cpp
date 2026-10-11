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

float smoothingAlpha(float dt, float tau) {
    return 1.f - std::exp(-dt / std::max(0.001f, tau));
}

} // namespace

AudioAnalyzer::AudioAnalyzer() {
    applyOptions(AudioAnalysisOptions{});
}

void AudioAnalyzer::setOptions(const AudioAnalysisOptions &options) {
    applyOptions(options);
}

void AudioAnalyzer::resetPublishSchedule() {
    _nextPublishSample = 0;
    _publishScheduleReady = false;
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
    resetDetectors();
    resetPublishSchedule();

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
    size_t kMaxEx = std::min(half, hzToBin(fMax) + 1);
    if (kMaxEx <= kMin + 1) {
        return;
    }

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
            edges[i] = edges[bandCount] > i ? edges[bandCount] - ((size_t)bandCount - i)
                                             : edges[i - 1];
        }
    }

    for (uint16_t b = 0; b < bandCount; ++b) {
        size_t k0 = edges[b];
        size_t k1 = edges[b + 1];
        if (k1 <= k0) {
            k1 = k0;
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

void AudioAnalyzer::resetDetectors() {
    _lowOnset.reset();
    _spectralFlux.reset();
    _bassRhythm.reset();
    _beatPulseEnv = 0.f;
    _beatStrengthHold = 0.f;
    _lowOnsetEnv = 0.f;
    _bassPulseEnv = 0.f;
    _bassStrengthHold = 0.f;
}

void AudioAnalyzer::reset(uint64_t sessionId, uint64_t samplePosition) {
    _sessionId = sessionId;
    _sequence = 0;
    _samplePos = samplePosition;
    clearRing();
    _smoothReady = false;
    std::fill(_smooth.begin(), _smooth.end(), 0.f);
    std::fill(_floor.begin(), _floor.end(), 0.f);
    resetDetectors();
    resetPublishSchedule();
}

void AudioAnalyzer::seek(uint64_t samplePosition) {
    _samplePos = samplePosition;
    clearRing();
    _smoothReady = false;
    std::fill(_smooth.begin(), _smooth.end(), 0.f);
    std::fill(_floor.begin(), _floor.end(), 0.f);
    resetDetectors();
    resetPublishSchedule();
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

void AudioAnalyzer::applyFloorRelative(float *bands, uint16_t bandCount, float deltaSeconds) {
    if (_floor.size() < bandCount) {
        _floor.assign(bandCount, 0.f);
    }
    for (uint16_t i = 0; i < bandCount; ++i) {
        float &fl = _floor[i];
        const float x = bands[i];
        const float tau = x > fl ? kFloorUpSec : kFloorDownSec;
        fl += (x - fl) * smoothingAlpha(deltaSeconds, tau);

        const float denom = std::max(1e-4f, 1.f - kFloorMix * fl);
        const float rel = std::clamp((x - kFloorMix * fl) / denom, 0.f, 1.f);
        bands[i] = std::clamp(rel * kDisplayGain, 0.f, 1.f);
    }
}

void AudioAnalyzer::smoothBands(float *bands, uint16_t bandCount, float deltaSeconds) {
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
        const float tau = tgt > cur ? kBandAttackSec : kBandReleaseSec;
        _smooth[i] = cur + (tgt - cur) * smoothingAlpha(deltaSeconds, tau);
        bands[i] = _smooth[i];
    }
}

void AudioAnalyzer::updateRhythmFeatures(AudioAnalysisFrame &out, float deltaSeconds) {
    const float pulseDecay = std::exp(-deltaSeconds / kPulseDecaySec);
    const float strengthDecay = std::exp(-deltaSeconds / kStrengthDecaySec);

    const auto flux = _spectralFlux.process(_mag.data(), _mag.size(), _sampleRate, _options.fftSize,
                                            deltaSeconds);
    const auto low = _lowOnset.process(_mag.data(), _mag.size(), _sampleRate, _options.fftSize,
                                       deltaSeconds);

    out.onsetStrength = flux.onsetStrength;
    out.lowEnergy = low.lowEnergy;
    out.lowOnsetStrength = low.onset;

    // 低频冲击包络：只在 hit 时冲顶，然后衰减（不每帧注入连续 onset）
    _lowOnsetEnv *= pulseDecay;
    _bassPulseEnv *= pulseDecay;
    if (low.hit && low.onset >= 0.35f) {
        _lowOnsetEnv = std::max(_lowOnsetEnv, low.onset);
        _bassPulseEnv = std::max(_bassPulseEnv, low.onset);
        _bassStrengthHold = low.onset;
        const double tSec =
            (double)out.samplePosition / (double)std::max<uint32_t>(1, out.sampleRate);
        _bassRhythm.onLowHit(tSec);
    }

    // 通用打击：仅 Spectral Flux；强度门槛集中在检测器内部，外层不再叠加
    _beatPulseEnv *= pulseDecay;
    if (flux.hit) {
        _beatPulseEnv = std::max(_beatPulseEnv, flux.pulse);
        _beatStrengthHold = flux.pulse;
    }

    _beatStrengthHold *= strengthDecay;
    _bassStrengthHold *= strengthDecay;
    if (_beatPulseEnv < 0.02f) {
        _beatPulseEnv = 0.f;
    }
    if (_lowOnsetEnv < 0.015f) {
        _lowOnsetEnv = 0.f;
    }
    if (_bassPulseEnv < 0.015f) {
        _bassPulseEnv = 0.f;
    }
    if (_beatStrengthHold < 0.03f) {
        _beatStrengthHold = 0.f;
    }
    if (_bassStrengthHold < 0.03f) {
        _bassStrengthHold = 0.f;
    }

    _bassRhythm.tick(deltaSeconds);

    out.beatPulse = std::clamp(_beatPulseEnv, 0.f, 1.f);
    out.beatStrength = std::clamp(_beatStrengthHold, 0.f, 1.f);
    out.lowOnset = std::clamp(_lowOnsetEnv, 0.f, 1.f);
    out.bassPulse = std::clamp(_bassPulseEnv, 0.f, 1.f);
    out.bassStrength = std::clamp(_bassStrengthHold, 0.f, 1.f);
    out.bassRhythmRate = _bassRhythm.rateHz();
    out.rhythmConfidence = _bassRhythm.confidence();
    out.beat = out.beatPulse;
}

bool AudioAnalyzer::shouldPublish(uint64_t center) {
    if (_options.outputRate == 0) {
        return true;
    }
    const uint64_t step = std::max<uint64_t>(1, (uint64_t)_sampleRate / _options.outputRate);
    if (!_publishScheduleReady) {
        _publishScheduleReady = true;
        _nextPublishSample = center + step;
        return true;
    }
    if (center < _nextPublishSample) {
        return false;
    }
    // 按计划时刻推进，避免从「实际输出时刻」重计时造成抖动
    do {
        _nextPublishSample += step;
    } while (_nextPublishSample <= center);
    return true;
}

bool AudioAnalyzer::processHop(AudioAnalysisFrame &out) {
    const size_t n = _options.fftSize;
    const size_t hop = _options.hopSize;
    if (_ringCount < n || _sampleRate == 0) {
        return false;
    }

    const uint64_t windowStart = _samplePos;
    const uint64_t center = windowStart + n / 2;
    const float deltaSeconds =
        std::clamp((float)hop / (float)_sampleRate, 1e-4f, 0.25f);

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

    // 每个 hop 都更新频谱平滑与节奏检测（即使本帧不发布）
    AudioAnalysisFrame frame{};
    frame.sessionId = _sessionId;
    frame.samplePosition = center; // 分析中心时间，供节奏对齐
    frame.sampleRate = _sampleRate;
    frame.rms = std::sqrt(rmsAcc / (float)n);
    frame.peak = peak;
    frame.bandCount = _options.bandCount;

    mapLogBands(frame.bands.data(), frame.bandCount);
    applyFloorRelative(frame.bands.data(), frame.bandCount, deltaSeconds);
    smoothBands(frame.bands.data(), frame.bandCount, deltaSeconds);

    const uint16_t nb = frame.bandCount;
    const uint16_t b1 = std::max<uint16_t>(1, nb / 6);
    const uint16_t b2 = std::max<uint16_t>((uint16_t)(b1 + 1), (uint16_t)(nb * 2 / 3));
    auto avg = [&](uint16_t from, uint16_t to) {
        float s = 0.f;
        uint16_t c = 0;
        for (uint16_t i = from; i < to && i < nb; ++i) {
            s += frame.bands[i];
            ++c;
        }
        return c ? s / (float)c : 0.f;
    };
    frame.bass = avg(0, b1);
    frame.mid = avg(b1, b2);
    frame.treble = avg(b2, nb);

    updateRhythmFeatures(frame, deltaSeconds);

    const size_t drop = std::min(hop, _ringCount);
    _ringCount -= drop;
    _samplePos += drop;

    if (!shouldPublish(center)) {
        return false;
    }

    frame.sequence = ++_sequence;
    out = frame;
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
        resetDetectors();
        resetPublishSchedule();
        _sampleRate = sampleRate;
        rebuildBandTables();
    }

    size_t produced = 0;
    size_t consumed = 0;
    // 写入与分析交错：避免超大块一次灌满 ring 覆盖未分析样点
    const size_t writeChunk = std::max<size_t>(1, _options.hopSize);

    auto drainHops = [&]() {
        while (_ringCount >= _options.fftSize) {
            const size_t before = _ringCount;
            AudioAnalysisFrame frame;
            if (processHop(frame)) {
                if (produced < maxOut) {
                    outFrames[produced++] = frame;
                }
                // maxOut 已满仍继续分析，保证检测不漏；多余发布帧丢弃
            }
            if (_ringCount >= before) {
                break; // 未消耗则退出，防止死循环
            }
        }
    };

    while (consumed < frameCount) {
        size_t freeSlots = (_ringCount < _ringCap) ? (_ringCap - _ringCount) : 0;
        if (freeSlots == 0) {
            drainHops();
            freeSlots = (_ringCount < _ringCap) ? (_ringCap - _ringCount) : 0;
            if (freeSlots == 0) {
                // ring 仍满：强制分析一窗腾位置；若仍无法则覆盖最旧（pushMono 行为）
                if (_ringCount >= _options.fftSize) {
                    AudioAnalysisFrame frame;
                    if (processHop(frame) && produced < maxOut) {
                        outFrames[produced++] = frame;
                    }
                }
                freeSlots = (_ringCount < _ringCap) ? (_ringCap - _ringCount) : writeChunk;
            }
        }

        const size_t nWrite = std::min({writeChunk, freeSlots, frameCount - consumed});
        if (channels == 1) {
            for (size_t i = 0; i < nWrite; ++i) {
                pushMono(samples[consumed + i] / 32768.f);
            }
        } else {
            for (size_t i = 0; i < nWrite; ++i) {
                float sum = 0.f;
                for (int c = 0; c < channels; ++c) {
                    sum += samples[(consumed + i) * (size_t)channels + (size_t)c] / 32768.f;
                }
                pushMono(sum / (float)channels);
            }
        }
        consumed += nWrite;
        drainHops();
    }

    drainHops();
    return produced;
}
