#include "Fft.h"

#include <cmath>
#include <stdexcept>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

inline bool isPowerOfTwo(size_t n) {
    return n >= 2 && (n & (n - 1)) == 0;
}

} // namespace

Fft::Fft(size_t fftSize) {
    resize(fftSize);
}

void Fft::resize(size_t fftSize) {
    if (!isPowerOfTwo(fftSize)) {
        throw std::invalid_argument(
            "Fft size must be a power of two"
        );
    }

    _n = fftSize;
    rebuildTables();
}

void Fft::rebuildTables() {
    // ------------------------------------------------------------
    // Bit-reversal table
    //
    // For example, N = 8:
    //
    //   0 -> 0
    //   1 -> 4
    //   2 -> 2
    //   3 -> 6
    //   4 -> 1
    //   5 -> 5
    //   6 -> 3
    //   7 -> 7
    // ------------------------------------------------------------

    _bitrev.resize(_n);

    size_t bits = 0;
    for (size_t value = _n; value > 1; value >>= 1) {
        ++bits;
    }

    for (size_t i = 0; i < _n; ++i) {
        size_t x = i;
        size_t reversed = 0;

        for (size_t bit = 0; bit < bits; ++bit) {
            reversed = (reversed << 1) | (x & 1);
            x >>= 1;
        }

        _bitrev[i] = reversed;
    }

    // ------------------------------------------------------------
    // Twiddle factors
    //
    // We use:
    //
    //   W_N^k = exp(-j * 2*pi*k/N)
    //
    // and store:
    //
    //   real, imaginary
    //
    // for every butterfly stage.
    // ------------------------------------------------------------

    _twiddle.clear();

    // Total number of twiddle pairs:
    //
    //   N/2 + N/2 + ... + N/2
    //   log2(N) stages
    //
    // Actually each stage has len/2 values, so total is N - 1.
    _twiddle.reserve((_n - 1) * 2);

    for (size_t len = 2; len <= _n; len <<= 1) {
        const size_t half = len >> 1;

        const double angleStep =
            -2.0 * M_PI / static_cast<double>(len);

        for (size_t k = 0; k < half; ++k) {
            const double angle =
                angleStep * static_cast<double>(k);

            _twiddle.push_back(
                static_cast<float>(std::cos(angle))
            );

            _twiddle.push_back(
                static_cast<float>(std::sin(angle))
            );
        }

        // Avoid overflow if size_t is extremely large.
        if (len > _n / 2) {
            break;
        }
    }
}

void Fft::forward(
    const float *in,
    float *outInterleaved
) const {
    if (in == nullptr || outInterleaved == nullptr) {
        throw std::invalid_argument(
            "Fft::forward input/output must not be null"
        );
    }

    // ------------------------------------------------------------
    // 1. Copy input into bit-reversed order.
    //
    // The input is real:
    //
    //   x[n]
    //
    // so the imaginary component is initialized to zero.
    //
    // Output layout:
    //
    //   [real0, imag0,
    //    real1, imag1,
    //    ...]
    // ------------------------------------------------------------

    for (size_t i = 0; i < _n; ++i) {
        const size_t j = _bitrev[i];

        outInterleaved[j * 2] = in[i];
        outInterleaved[j * 2 + 1] = 0.0f;
    }

    // ------------------------------------------------------------
    // 2. Iterative radix-2 Cooley-Tukey FFT.
    // ------------------------------------------------------------

    size_t twiddleOffset = 0;

    for (size_t len = 2; len <= _n; len <<= 1) {
        const size_t half = len >> 1;

        for (size_t block = 0; block < _n; block += len) {
            for (size_t k = 0; k < half; ++k) {
                const size_t twiddleIndex =
                    twiddleOffset + k * 2;

                const float wr =
                    _twiddle[twiddleIndex];

                const float wi =
                    _twiddle[twiddleIndex + 1];

                const size_t even =
                    (block + k) * 2;

                const size_t odd =
                    (block + k + half) * 2;

                const float oddReal =
                    outInterleaved[odd];

                const float oddImag =
                    outInterleaved[odd + 1];

                // Complex multiplication:
                //
                // (wr + j*wi) * (oddReal + j*oddImag)
                //
                // real = wr*oddReal - wi*oddImag
                // imag = wr*oddImag + wi*oddReal

                const float tempReal =
                    wr * oddReal - wi * oddImag;

                const float tempImag =
                    wr * oddImag + wi * oddReal;

                const float evenReal =
                    outInterleaved[even];

                const float evenImag =
                    outInterleaved[even + 1];

                // Butterfly:
                //
                // X[k]       = E[k] + W[k] O[k]
                // X[k+N/2]   = E[k] - W[k] O[k]

                outInterleaved[even] =
                    evenReal + tempReal;

                outInterleaved[even + 1] =
                    evenImag + tempImag;

                outInterleaved[odd] =
                    evenReal - tempReal;

                outInterleaved[odd + 1] =
                    evenImag - tempImag;
            }
        }

        twiddleOffset += half * 2;

        // Prevent size_t overflow.
        if (len > _n / 2) {
            break;
        }
    }
}