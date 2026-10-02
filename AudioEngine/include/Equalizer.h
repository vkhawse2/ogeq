// Equalizer.h -- parametric/graphic EQ built on BiquadFilter.
//
// Real-time safe: setBand() recomputes coefficients on the control thread;
// process() only runs the difference equations. Channel count is fixed at
// configure() time; each channel owns its own filter state.

#pragma once

#include "BiquadFilter.h"
#include <array>
#include <cstdint>

namespace ogeq {

constexpr int kMaxBands = 10;
constexpr int kMaxChannels = 8;

enum class FilterType : uint8_t {
    Peaking = 0,
    LowShelf,
    HighShelf,
    LowPass,
    HighPass,
};

struct EqBand {
    FilterType type = FilterType::Peaking;
    float freqHz = 1000.0f;
    float q = 1.0f;
    float gainDb = 0.0f;
    bool enabled = true;
};

class Equalizer {
public:
    Equalizer();

    // Control thread: sample rate in Hz, channels 1..kMaxChannels.
    void configure(float sampleRate, int channels, int numBands);

    // Control thread: update one band (recomputes coefficients).
    void setBand(int index, const EqBand& band);

    // Control thread.
    void setPreampDb(float db) { preampDb_ = db; }
    void setBypass(bool bypass) { bypass_ = bypass; }

    // Real-time thread: interleaved float32 in/out, frames samples per channel.
    // In-place safe (in == out allowed).
    void process(const float* in, float* out, uint32_t frames) noexcept;

    // Real-time safe.
    void reset() noexcept;

private:
    void updateCoefficients() noexcept;

    float sampleRate_ = 48000.0f;
    int channels_ = 2;
    int numBands_ = 5;
    float preampDb_ = 0.0f;
    bool bypass_ = false;

    std::array<EqBand, kMaxBands> bands_;
    // [channel][band]
    Biquad filters_[kMaxChannels][kMaxBands];
};

} // namespace ogeq
