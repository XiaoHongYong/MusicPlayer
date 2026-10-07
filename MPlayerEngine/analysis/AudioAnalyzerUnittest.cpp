#include "AudioAnalyzer.h"
#include "Fft.h"

#if UNIT_TEST

#include "utils/unittest.h"

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

#endif // UNIT_TEST
