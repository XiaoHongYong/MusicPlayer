#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// 实数输入的 radix-2 FFT（输出交错 re/im，长度 fftSize）。
// 仅支持 2 的幂次尺寸。
class Fft {
public:
    explicit Fft(size_t fftSize = 2048);

    void configure(size_t fftSize) {
        if (fftSize < 2 || (fftSize & (fftSize - 1)) != 0) {
            throw std::invalid_argument("FFT size must be a power of two");
        }
        _n = fftSize;
        rebuildTables();
    }

    void resize(size_t fftSize);
    size_t size() const { return _n; }

    // in[0..n)：时域实数；out[0..n*2)：交错复数 re,im
    void forward(const float *in, float *outInterleaved) const;

private:
    void rebuildTables();

    size_t _n = 0;
    std::vector<size_t> _bitrev;
    std::vector<float> _twiddle; // cos/sin pairs for each stage
};
