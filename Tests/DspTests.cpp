// DspTests.cpp -- unit tests for the OGEQ DSP engine.
//
// Phase 1 acceptance:
//  1. Biquad peaking filter produces the designed gain at center frequency.
//  2. Bypass is bit-transparent.
//  3. No clipping explosion: unity EQ on full-scale sine stays bounded.
//  4. Performance: 10 bands x stereo @ 48kHz processes 1s of audio fast
//     enough for real-time (target: < 5% of real time on CI hardware).
//
// Minimal test framework (no external deps): ASSERT macros + main().

#include "BiquadFilter.h"
#include "DspProcessor.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <chrono>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond) do { \
    ++g_checks; \
    if (!(cond)) { \
        ++g_failures; \
        std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define CHECK_CLOSE(a, b, tol) do { \
    ++g_checks; \
    double _da = (double)(a), _db = (double)(b); \
    if (std::fabs(_da - _db) > (tol)) { \
        ++g_failures; \
        std::printf("FAIL %s:%d: |%f - %f| > %f\n", __FILE__, __LINE__, _da, _db, (double)(tol)); \
    } \
} while (0)

namespace {

// Measure RMS of a buffer.
float rms(const float* buf, size_t n) {
    double sum = 0;
    for (size_t i = 0; i < n; ++i) sum += (double)buf[i] * buf[i];
    return (float)std::sqrt(sum / (double)n);
}

// Generate a sine at freq Hz, sampleRate, n samples.
void sine(float* buf, size_t n, float freq, float sampleRate, float amp = 0.5f) {
    for (size_t i = 0; i < n; ++i)
        buf[i] = amp * std::sin(2.0f * 3.14159265f * freq * (float)i / sampleRate);
}

} // namespace

void test_peaking_gain() {
    // +6 dB peaking at 1 kHz, Q=1. A 1 kHz sine should come out ~2x amplitude.
    ogeq::Biquad f;
    f.designPeaking(48000.0f, 1000.0f, 1.0f, 6.0f);

    const size_t n = 48000;
    std::vector<float> in(n), out(n);
    sine(in.data(), n, 1000.0f, 48000.0f);

    // Warm up (let transient settle), then measure.
    for (size_t i = 0; i < n; ++i) out[i] = f.process(in[i]);
    float inRms = rms(in.data() + 24000, 24000);
    float outRms = rms(out.data() + 24000, 24000);
    float gainDb = 20.0f * std::log10(outRms / inRms);
    std::printf("  peaking +6dB measured: %.2f dB\n", gainDb);
    CHECK_CLOSE(gainDb, 6.0, 0.5);
}

void test_bypass_transparent() {
    ogeq::DspProcessor dsp;
    ogeq::DspSettings s;
    s.eqBypass = true;
    s.numBands = 5;
    dsp.applySettings(s);
    dsp.configure(48000.0f, 2);

    const size_t frames = 1024;
    std::vector<float> in(frames * 2), out(frames * 2);
    for (size_t i = 0; i < in.size(); ++i) in[i] = (float)i / (float)in.size() * 2.0f - 1.0f;

    dsp.process(in.data(), out.data(), frames);
    for (size_t i = 0; i < in.size(); ++i)
        CHECK_CLOSE(out[i], in[i], 1e-6);
    std::printf("  bypass transparent: ok\n");
}

void test_stability() {
    // Extreme settings should not explode: +12 dB on all 5 bands, full-scale in.
    ogeq::DspProcessor dsp;
    ogeq::DspSettings s;
    s.numBands = 5;
    const float freqs[5] = {60, 230, 910, 3600, 14000};
    for (int i = 0; i < 5; ++i) {
        s.bands[i].type = ogeq::FilterType::Peaking;
        s.bands[i].freqHz = freqs[i];
        s.bands[i].q = 1.0f;
        s.bands[i].gainDb = 12.0f;
        s.bands[i].enabled = true;
    }
    dsp.applySettings(s);
    dsp.configure(48000.0f, 2);

    const size_t frames = 48000;
    std::vector<float> in(frames * 2), out(frames * 2);
    sine(in.data(), frames * 2, 440.0f, 48000.0f, 1.0f); // full scale

    dsp.process(in.data(), out.data(), frames);

    float peak = 0;
    for (float v : out) {
        CHECK(std::isfinite(v)); // no NaN/Inf
        if (std::fabs(v) > peak) peak = std::fabs(v);
    }
    std::printf("  stability peak: %.2f (finite: ok)\n", peak);
}

void test_performance() {
    ogeq::DspProcessor dsp;
    ogeq::DspSettings s;
    s.numBands = 10;
    for (int i = 0; i < 10; ++i) {
        s.bands[i].type = ogeq::FilterType::Peaking;
        s.bands[i].freqHz = 31.0f * std::pow(2.0f, (float)i);
        s.bands[i].q = 1.0f;
        s.bands[i].gainDb = 3.0f;
        s.bands[i].enabled = true;
    }
    dsp.applySettings(s);
    dsp.configure(48000.0f, 2);

    const size_t frames = 48000; // 1 second stereo
    std::vector<float> in(frames * 2, 0.1f), out(frames * 2);

    auto t0 = std::chrono::high_resolution_clock::now();
    const int reps = 20;
    for (int r = 0; r < reps; ++r)
        dsp.process(in.data(), out.data(), frames);
    auto t1 = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / reps;
    // 1 second of audio must process in well under 1000 ms; target < 50 ms.
    std::printf("  perf: %.2f ms per second of audio (10 bands stereo)\n", ms);
    CHECK(ms < 50.0);
}

int main() {
    std::printf("OGEQ DSP tests\n");
    std::printf(" test_peaking_gain...\n");      test_peaking_gain();
    std::printf(" test_bypass_transparent...\n"); test_bypass_transparent();
    std::printf(" test_stability...\n");          test_stability();
    std::printf(" test_performance...\n");        test_performance();
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
