// Equalizer.cpp

#include "Equalizer.h"
#include <cmath>

namespace ogeq {

Equalizer::Equalizer() {
    // Default: 5-band layout.
    const float defaults[kMaxBands] = {60, 230, 910, 3600, 14000};
    for (int i = 0; i < kMaxBands; ++i) {
        bands_[i].freqHz = defaults[i % 5];
        bands_[i].q = 1.0f;
        bands_[i].gainDb = 0.0f;
        bands_[i].type = FilterType::Peaking;
        bands_[i].enabled = true;
    }
    updateCoefficients();
}

void Equalizer::configure(float sampleRate, int channels, int numBands) {
    if (sampleRate > 0) sampleRate_ = sampleRate;
    if (channels >= 1 && channels <= kMaxChannels) channels_ = channels;
    if (numBands >= 1 && numBands <= kMaxBands) numBands_ = numBands;
    updateCoefficients();
    reset();
}

void Equalizer::setBand(int index, const EqBand& band) {
    if (index < 0 || index >= kMaxBands) return;
    bands_[index] = band;
    updateCoefficients();
}

void Equalizer::updateCoefficients() noexcept {
    for (int ch = 0; ch < channels_; ++ch) {
        for (int b = 0; b < numBands_; ++b) {
            const EqBand& band = bands_[b];
            Biquad& f = filters_[ch][b];
            if (!band.enabled || band.gainDb == 0.0f) {
                // Unity: pass-through.
                f.b0 = 1.0f; f.b1 = 0.0f; f.b2 = 0.0f;
                f.a1 = 0.0f; f.a2 = 0.0f;
                continue;
            }
            switch (band.type) {
            case FilterType::Peaking:
                f.designPeaking(sampleRate_, band.freqHz, band.q, band.gainDb);
                break;
            case FilterType::LowShelf:
                f.designLowShelf(sampleRate_, band.freqHz, band.gainDb);
                break;
            case FilterType::HighShelf:
                f.designHighShelf(sampleRate_, band.freqHz, band.gainDb);
                break;
            case FilterType::LowPass:
                f.designLowPass(sampleRate_, band.freqHz, band.q);
                break;
            case FilterType::HighPass:
                f.designHighPass(sampleRate_, band.freqHz, band.q);
                break;
            }
        }
    }
}

void Equalizer::process(const float* in, float* out, uint32_t frames) noexcept {
    if (bypass_ || numBands_ <= 0) {
        if (in != out) {
            for (uint32_t i = 0; i < frames * (uint32_t)channels_; ++i)
                out[i] = in[i];
        }
        return;
    }

    const float preamp = std::pow(10.0f, preampDb_ / 20.0f);

    for (uint32_t i = 0; i < frames; ++i) {
        for (int ch = 0; ch < channels_; ++ch) {
            float s = in[i * channels_ + ch] * preamp;
            for (int b = 0; b < numBands_; ++b) {
                s = filters_[ch][b].process(s);
            }
            out[i * channels_ + ch] = s;
        }
    }
}

void Equalizer::reset() noexcept {
    for (int ch = 0; ch < kMaxChannels; ++ch)
        for (int b = 0; b < kMaxBands; ++b)
            filters_[ch][b].reset();
}

} // namespace ogeq
