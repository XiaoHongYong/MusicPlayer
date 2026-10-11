#include "AudioAnalyzer.h"
#include "BassRhythmEstimator.h"
#include "Fft.h"
#include "LowOnsetDetector.h"
#include "SpectralFluxOnsetDetector.h"

#if UNIT_TEST

#include "utils/unittest.h"

#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(AudioAnalysisFft, SinePeakBin) {
    const size_t n = 2048;
    Fft fft(n);
    std::vector<float> in(n), out(n * 2);
    const float freqBin = 10.f;
    for (size_t i = 0; i < n; ++i) {
        in[i] = std::sin(2.f * float(M_PI) * freqBin * float(i) / float(n));
    }
    fft.forward(in.data(), out.data());

    size_t peak = 0;
    float peakMag = 0.f;
    for (size_t i = 0; i < n / 2; ++i) {
        const float re = out[i * 2];
        const float im = out[i * 2 + 1];
        const float mag = std::sqrt(re * re + im * im);
        if (mag > peakMag) {
            peakMag = mag;
            peak = i;
        }
    }
    EXPECT_NEAR((double)peak, 10.0, 1.0);
}

TEST(AudioAnalyzer, ProducesFramesFromSine) {
    AudioAnalyzer analyzer;
    AudioAnalysisOptions opt;
    opt.fftSize = 1024;
    opt.hopSize = 512;
    opt.bandCount = 32;
    opt.outputRate = 60;
    analyzer.setOptions(opt);
    analyzer.reset(7, 0);

    const int sr = 44100;
    const int channels = 2;
    const size_t frames = 4096;
    std::vector<int16_t> pcm(frames * channels);
    for (size_t i = 0; i < frames; ++i) {
        const float s = 0.4f * std::sin(2.f * float(M_PI) * 440.f * float(i) / float(sr));
        const int16_t v = (int16_t)(s * 32767.f);
        pcm[i * 2] = v;
        pcm[i * 2 + 1] = v;
    }

    AudioAnalysisFrame out[16];
    const size_t n = analyzer.pushInterleavedS16(pcm.data(), frames, channels, sr, out, 16);
    ASSERT_GT(n, 0u);
    EXPECT_EQ(out[0].sessionId, 7u);
    EXPECT_EQ(out[0].sampleRate, (uint32_t)sr);
    EXPECT_EQ(out[0].bandCount, 32);
    EXPECT_GT(out[0].rms, 0.01f);
    EXPECT_GT(out[0].peak, 0.01f);

    // 440 Hz 应落在 mid 附近，且至少有一个频带明显抬起
    float maxBand = 0.f;
    for (uint16_t i = 0; i < out[0].bandCount; ++i) {
        if (out[0].bands[i] > maxBand) {
            maxBand = out[0].bands[i];
        }
    }
    EXPECT_GT(maxBand, 0.15f);
    EXPECT_GT(out[0].mid, out[0].bass * 0.5f);

    float sum = 0.f;
    for (uint16_t i = 0; i < out[0].bandCount; ++i) {
        sum += out[0].bands[i];
    }
    const float mean = sum / (float)out[0].bandCount;
    EXPECT_GT(maxBand, mean * 1.8f);
}

TEST(AudioAnalyzer, SessionResetDropsOldTimeline) {
    AudioAnalyzer analyzer;
    AudioAnalysisOptions opt;
    opt.fftSize = 512;
    opt.hopSize = 256;
    opt.bandCount = 16;
    opt.outputRate = 120;
    analyzer.setOptions(opt);
    analyzer.reset(1, 0);

    std::vector<int16_t> pcm(2048, 1000);
    AudioAnalysisFrame out[8];
    analyzer.pushInterleavedS16(pcm.data(), pcm.size(), 1, 48000, out, 8);

    analyzer.reset(2, 1000);
    const size_t n = analyzer.pushInterleavedS16(pcm.data(), pcm.size(), 1, 48000, out, 8);
    ASSERT_GT(n, 0u);
    EXPECT_EQ(out[0].sessionId, 2u);
    EXPECT_GE(out[0].samplePosition, 1000u);
}

TEST(AudioAnalyzer, SilenceNearZero) {
    AudioAnalyzer analyzer;
    AudioAnalysisOptions opt;
    opt.fftSize = 512;
    opt.hopSize = 256;
    opt.bandCount = 16;
    opt.outputRate = 120;
    analyzer.setOptions(opt);
    analyzer.reset(1, 0);

    std::vector<int16_t> pcm(2048, 0);
    AudioAnalysisFrame out[8];
    const size_t n = analyzer.pushInterleavedS16(pcm.data(), pcm.size(), 1, 48000, out, 8);
    ASSERT_GT(n, 0u);
    EXPECT_LT(out[0].rms, 0.01f);
    EXPECT_LT(out[0].peak, 0.01f);
    for (uint16_t i = 0; i < out[0].bandCount; ++i) {
        EXPECT_LT(out[0].bands[i], 0.05f);
    }
}

TEST(LowOnsetDetector, KickJumpTriggersHit) {
    LowOnsetDetector det;
    const uint32_t sr = 48000;
    const size_t fftSize = 2048;
    const size_t bins = fftSize / 2;
    std::vector<float> mag(bins, 0.01f);

    for (int i = 0; i < 40; ++i) {
        det.process(mag.data(), mag.size(), sr, fftSize, 0.021f);
    }

    for (size_t k = 3; k <= 8; ++k) {
        mag[k] = 1.4f;
    }
    bool hit = false;
    float maxOnset = 0.f;
    for (int i = 0; i < 5; ++i) {
        const auto r = det.process(mag.data(), mag.size(), sr, fftSize, 0.021f);
        maxOnset = std::max(maxOnset, r.onset);
        if (r.hit) {
            hit = true;
        }
    }
    EXPECT_TRUE(hit);
    EXPECT_GT(maxOnset, 0.25f);
}

TEST(LowOnsetDetector, TinyLeakageDoesNotSaturateOnset) {
    // 基线接近静音时，微小频谱泄漏不应把 dB 起音顶满
    LowOnsetDetector det;
    const uint32_t sr = 48000;
    const size_t fftSize = 2048;
    std::vector<float> mag(fftSize / 2, 1e-6f);

    for (int i = 0; i < 30; ++i) {
        det.process(mag.data(), mag.size(), sr, fftSize, 0.021f);
    }
    for (size_t k = 3; k <= 8; ++k) {
        mag[k] = 2e-5f; // 仍远低于 minLowEnergyDb
    }
    float maxOnset = 0.f;
    bool anyHit = false;
    for (int i = 0; i < 10; ++i) {
        const auto r = det.process(mag.data(), mag.size(), sr, fftSize, 0.021f);
        maxOnset = std::max(maxOnset, r.onset);
        anyHit = anyHit || r.hit;
        EXPECT_LT(r.lowEnergy, 0.15f);
    }
    EXPECT_FALSE(anyHit);
    EXPECT_LT(maxOnset, 0.05f);
}

TEST(LowOnsetDetector, MidBandEnergyKeepsLowEnergyLow) {
    // 只抬升 ~500Hz 一带，40–200Hz lowEnergy 应接近 0
    LowOnsetDetector det;
    const uint32_t sr = 44100;
    const size_t fftSize = 2048;
    const float binHz = (float)sr / (float)fftSize;
    std::vector<float> mag(fftSize / 2, 0.f);
    const size_t midBin = (size_t)std::lround(500.f / binHz);
    ASSERT_LT(midBin, mag.size());
    mag[midBin] = 2.f;

    float maxLow = 0.f;
    for (int i = 0; i < 20; ++i) {
        maxLow = std::max(maxLow, det.process(mag.data(), mag.size(), sr, fftSize, 0.021f).lowEnergy);
    }
    EXPECT_LT(maxLow, 0.12f);
}

TEST(LowOnsetDetector, SteadyAboveThresholdDoesNotSpamHits) {
    LowOnsetDetector det;
    const uint32_t sr = 48000;
    const size_t fftSize = 2048;
    std::vector<float> mag(fftSize / 2, 0.02f);

    for (int i = 0; i < 20; ++i) {
        det.process(mag.data(), mag.size(), sr, fftSize, 0.021f);
    }
    // 一次抬升后保持高位：阈值上穿只应触发有限次数（上穿 + refractory）
    for (size_t k = 3; k <= 8; ++k) {
        mag[k] = 1.2f;
    }
    int hits = 0;
    for (int i = 0; i < 80; ++i) {
        if (det.process(mag.data(), mag.size(), sr, fftSize, 0.021f).hit) {
            ++hits;
        }
    }
    EXPECT_GE(hits, 1);
    EXPECT_LE(hits, 3);
}

TEST(SpectralFluxOnsetDetector, SpectrumJumpTriggers) {
    SpectralFluxOnsetDetector det;
    const uint32_t sr = 48000;
    const size_t fftSize = 1024;
    const size_t bins = fftSize / 2;
    std::vector<float> mag(bins, 0.05f);
    for (int i = 0; i < 40; ++i) {
        det.process(mag.data(), mag.size(), sr, fftSize, 0.021f);
    }
    // 抬升 200Hz–1kHz 一带（节拍向频段内）
    for (size_t k = 4; k < 22; ++k) {
        mag[k] = 1.2f;
    }
    bool hit = false;
    // 峰值确认有 1 帧延迟，多跑几帧
    for (int i = 0; i < 8; ++i) {
        if (i == 2) {
            // 回落以触发峰值确认
            for (size_t k = 4; k < 22; ++k) {
                mag[k] = 0.08f;
            }
        }
        if (det.process(mag.data(), mag.size(), sr, fftSize, 0.021f).hit) {
            hit = true;
            break;
        }
    }
    EXPECT_TRUE(hit);
}

TEST(BassRhythmEstimator, StableIntervalsRaiseConfidence) {
    BassRhythmEstimator est;
    for (int i = 0; i < 6; ++i) {
        est.onLowHit(i * 0.5); // 2 Hz
    }
    EXPECT_NEAR(est.rateHz(), 2.0, 0.15);
    EXPECT_GT(est.confidence(), 0.5f);
}

TEST(AudioAnalyzer, LowPulseRaisesBassNotOnlyFlux) {
    AudioAnalyzer analyzer;
    AudioAnalysisOptions opt;
    opt.fftSize = 1024;
    opt.hopSize = 512;
    opt.bandCount = 32;
    opt.outputRate = 60;
    analyzer.setOptions(opt);
    analyzer.reset(9, 0);

    const int sr = 48000;
    const size_t frames = 8192;
    std::vector<int16_t> pcm(frames, 0);
    // 前半安静，后半注入 60Hz 突发
    for (size_t i = frames / 2; i < frames; ++i) {
        const float env = (i < frames / 2 + 256) ? ((float)(i - frames / 2) / 256.f) : 1.f;
        const float s = 0.7f * env * std::sin(2.f * float(M_PI) * 60.f * float(i) / float(sr));
        pcm[i] = (int16_t)(s * 32767.f);
    }

    AudioAnalysisFrame out[32];
    const size_t n = analyzer.pushInterleavedS16(pcm.data(), frames, 1, sr, out, 32);
    ASSERT_GT(n, 0u);
    float maxLowOnset = 0.f;
    float maxBassPulse = 0.f;
    float maxLowEnergy = 0.f;
    for (size_t i = 0; i < n; ++i) {
        maxLowOnset = std::max(maxLowOnset, out[i].lowOnset);
        maxBassPulse = std::max(maxBassPulse, out[i].bassPulse);
        maxLowEnergy = std::max(maxLowEnergy, out[i].lowEnergy);
        EXPECT_FLOAT_EQ(out[i].beat, out[i].beatPulse);
    }
    EXPECT_GT(maxLowEnergy, 0.15f);
    EXPECT_GT(maxLowOnset + maxBassPulse, 0.05f);
}

TEST(AudioAnalyzer, MidToneKeepsBassPulseQuiet) {
    // 1 kHz 正弦：lowEnergy / bassPulse 应接近 0（与通用 Flux 无关）
    AudioAnalyzer analyzer;
    AudioAnalysisOptions opt;
    opt.fftSize = 2048;
    opt.hopSize = 1024;
    opt.bandCount = 32;
    opt.outputRate = 60;
    analyzer.setOptions(opt);
    analyzer.reset(11, 0);

    const int sr = 44100;
    const size_t frames = 16384;
    std::vector<int16_t> pcm(frames);
    for (size_t i = 0; i < frames; ++i) {
        const float s = 0.55f * std::sin(2.f * float(M_PI) * 1000.f * float(i) / float(sr));
        pcm[i] = (int16_t)(s * 32767.f);
    }

    AudioAnalysisFrame out[48];
    const size_t n = analyzer.pushInterleavedS16(pcm.data(), frames, 1, sr, out, 48);
    ASSERT_GT(n, 0u);
    float maxLowEnergy = 0.f;
    float maxBassPulse = 0.f;
    float maxLowOnset = 0.f;
    for (size_t i = 0; i < n; ++i) {
        maxLowEnergy = std::max(maxLowEnergy, out[i].lowEnergy);
        maxBassPulse = std::max(maxBassPulse, out[i].bassPulse);
        maxLowOnset = std::max(maxLowOnset, out[i].lowOnset);
    }
    EXPECT_LT(maxLowEnergy, 0.2f);
    EXPECT_LT(maxBassPulse, 0.05f);
    EXPECT_LT(maxLowOnset, 0.05f);
}

#endif // UNIT_TEST
