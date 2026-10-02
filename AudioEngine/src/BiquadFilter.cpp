// BiquadFilter.cpp -- RBJ cookbook coefficient designs.

#include "BiquadFilter.h"
#include <cmath>

namespace ogeq {

static constexpr float kPi = 3.14159265358979323846f;

void Biquad::designPeaking(float sr, float freq, float q, float gainDb) noexcept {
    const float A = std::pow(10.0f, gainDb / 40.0f);
    const float w0 = 2.0f * kPi * freq / sr;
    const float alpha = std::sin(w0) / (2.0f * q);
    const float cw0 = std::cos(w0);

    const float b0 = 1.0f + alpha * A;
    const float b1 = -2.0f * cw0;
    const float b2 = 1.0f - alpha * A;
    const float a0 = 1.0f + alpha / A;
    const float a1 = -2.0f * cw0;
    const float a2 = 1.0f - alpha / A;

    this->b0 = b0 / a0; this->b1 = b1 / a0; this->b2 = b2 / a0;
    this->a1 = a1 / a0; this->a2 = a2 / a0;
}

void Biquad::designLowShelf(float sr, float freq, float gainDb) noexcept {
    const float A = std::pow(10.0f, gainDb / 40.0f);
    const float w0 = 2.0f * kPi * freq / sr;
    const float alpha = std::sin(w0) / 2.0f * std::sqrt(2.0f);
    const float cw0 = std::cos(w0);
    const float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;

    const float b0 = A * ((A + 1.0f) - (A - 1.0f) * cw0 + twoSqrtAAlpha);
    const float b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cw0);
    const float b2 = A * ((A + 1.0f) - (A - 1.0f) * cw0 - twoSqrtAAlpha);
    const float a0 = (A + 1.0f) + (A - 1.0f) * cw0 + twoSqrtAAlpha;
    const float a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cw0);
    const float a2 = (A + 1.0f) + (A - 1.0f) * cw0 - twoSqrtAAlpha;

    this->b0 = b0 / a0; this->b1 = b1 / a0; this->b2 = b2 / a0;
    this->a1 = a1 / a0; this->a2 = a2 / a0;
}

void Biquad::designHighShelf(float sr, float freq, float gainDb) noexcept {
    const float A = std::pow(10.0f, gainDb / 40.0f);
    const float w0 = 2.0f * kPi * freq / sr;
    const float alpha = std::sin(w0) / 2.0f * std::sqrt(2.0f);
    const float cw0 = std::cos(w0);
    const float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;

    const float b0 = A * ((A + 1.0f) + (A - 1.0f) * cw0 + twoSqrtAAlpha);
    const float b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cw0);
    const float b2 = A * ((A + 1.0f) + (A - 1.0f) * cw0 - twoSqrtAAlpha);
    const float a0 = (A + 1.0f) - (A - 1.0f) * cw0 + twoSqrtAAlpha;
    const float a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cw0);
    const float a2 = (A + 1.0f) - (A - 1.0f) * cw0 - twoSqrtAAlpha;

    this->b0 = b0 / a0; this->b1 = b1 / a0; this->b2 = b2 / a0;
    this->a1 = a1 / a0; this->a2 = a2 / a0;
}

void Biquad::designLowPass(float sr, float freq, float q) noexcept {
    const float w0 = 2.0f * kPi * freq / sr;
    const float alpha = std::sin(w0) / (2.0f * q);
    const float cw0 = std::cos(w0);

    const float b1 = 1.0f - cw0;
    const float b0 = b1 / 2.0f;
    const float b2 = b0;
    const float a0 = 1.0f + alpha;
    const float a1 = -2.0f * cw0;
    const float a2 = 1.0f - alpha;

    this->b0 = b0 / a0; this->b1 = b1 / a0; this->b2 = b2 / a0;
    this->a1 = a1 / a0; this->a2 = a2 / a0;
}

void Biquad::designHighPass(float sr, float freq, float q) noexcept {
    const float w0 = 2.0f * kPi * freq / sr;
    const float alpha = std::sin(w0) / (2.0f * q);
    const float cw0 = std::cos(w0);

    const float b1 = -(1.0f + cw0);
    const float b0 = -b1 / 2.0f;
    const float b2 = b0;
    const float a0 = 1.0f + alpha;
    const float a1 = -2.0f * cw0;
    const float a2 = 1.0f - alpha;

    this->b0 = b0 / a0; this->b1 = b1 / a0; this->b2 = b2 / a0;
    this->a1 = a1 / a0; this->a2 = a2 / a0;
}

} // namespace ogeq
