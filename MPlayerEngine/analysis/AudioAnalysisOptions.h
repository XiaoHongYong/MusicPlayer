#pragma once

#include <cstdint>

struct AudioAnalysisOptions {
    uint16_t fftSize = 2048;
    uint16_t hopSize = 1024;
    uint16_t bandCount = 64;
    uint16_t outputRate = 30; // 每秒最多产出多少帧（Hz）
};
