#pragma once

#include <array>
#include <cstdint>

struct AudioAnalysisFrame {
    static constexpr size_t kMaxBands = 128;

    uint64_t sessionId = 0;
    uint64_t sequence = 0;

    uint64_t samplePosition = 0;
    uint32_t sampleRate = 0;

    float rms = 0;
    float peak = 0;
    float bass = 0;
    float mid = 0;
    float treble = 0;
    float beat = 0;

    uint16_t bandCount = 0;
    std::array<float, kMaxBands> bands{};
};
