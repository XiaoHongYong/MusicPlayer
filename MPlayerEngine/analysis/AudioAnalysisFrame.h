#pragma once

#include <array>
#include <cstdint>

struct AudioAnalysisFrame {
    static constexpr size_t kMaxBands = 128;

    uint64_t sessionId = 0;
    uint64_t sequence = 0;

    uint64_t samplePosition = 0; // 分析窗中心样点
    uint32_t sampleRate = 0;

    float rms = 0;
    float peak = 0;
    float bass = 0;
    float mid = 0;
    float treble = 0;

    /** @deprecated 兼容旧消费者；等于 beatPulse */
    float beat = 0;

    // —— 通用起音 / 打击（约 80–3000Hz Flux，可含中频）——
    float onsetStrength = 0; // 连续 Spectral Flux
    float beatPulse = 0;     // 通用打击脉冲（onset-driven）
    float beatStrength = 0;  // 最近一次通用打击强度

    // —— 低频专属（约 40–200Hz）——
    float lowEnergy = 0;         // 固定 dB 映射的低频能量
    float lowOnsetStrength = 0;  // 连续低频起音（调试/插值）
    float lowOnset = 0;          // 仅 hit 触发的低频冲击包络
    float bassPulse = 0;         // 仅低频 hit 的脉冲（封面缩放等）
    float bassStrength = 0;      // 最近一次低频冲击强度
    float bassRhythmRate = 0;    // 低频事件频率估计 Hz（≠ BPM）
    float rhythmConfidence = 0;

    uint16_t bandCount = 0;
    std::array<float, kMaxBands> bands{};
};
