// BiquadFilter.h -- second-order IIR filter section for the OGEQ DSP engine.
//
// One biquad per band per channel. Coefficients are computed by the caller
// (see Equalizer.h) for the target sample rate; process() is real-time safe:
// no allocation, no branches on config, just the difference equation.

#pragma once

namespace ogeq {

struct Biquad {
    // Coefficients (set by design functions, not per-sample).
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;

    // State (per channel -- do not share across channels).
    float x1 = 0.0f, x2 = 0.0f;
    float y1 = 0.0f, y2 = 0.0f;

    inline float process(float x) noexcept {
        const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return y;
    }

    void reset() noexcept { x1 = x2 = y1 = y2 = 0.0f; }

    // RBJ cookbook designs. freq in Hz, q dimensionless, gainDb for shelves/peaks.
    void designPeaking(float sampleRate, float freq, float q, float gainDb) noexcept;
    void designLowShelf(float sampleRate, float freq, float gainDb) noexcept;
    void designHighShelf(float sampleRate, float freq, float gainDb) noexcept;
    void designLowPass(float sampleRate, float freq, float q) noexcept;
    void designHighPass(float sampleRate, float freq, float q) noexcept;
};

} // namespace ogeq
