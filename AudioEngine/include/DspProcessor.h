// DspProcessor.h -- top-level DSP chain for OGEQ.
//
// Order: preamp -> EQ -> (future: compressor/limiter -> spatial) -> output.
// Every stage is optional and bypassable. The APO owns one DspProcessor and
// calls process() from APOProcess. Settings arrive via applySettings() on the
// control thread using a lock-free parameter exchange (no blocking in RT).

#pragma once

#include "Equalizer.h"

namespace ogeq {

struct DspSettings {
    float preampDb = 0.0f;
    bool eqBypass = false;
    int numBands = 5;
    EqBand bands[kMaxBands];
    // Phase 4: spatial settings go here (bypassed by default).
};

class DspProcessor {
public:
    DspProcessor();

    // Control thread: push new settings (lock-free handoff to RT).
    void applySettings(const DspSettings& settings);

    // Control thread: configure for a new stream format.
    void configure(float sampleRate, int channels);

    // Real-time thread.
    void process(const float* in, float* out, uint32_t frames) noexcept;

private:
    Equalizer eq_;
    // Double-buffered settings for lock-free RT handoff.
    DspSettings pending_;
    bool pendingDirty_ = false;
};

} // namespace ogeq
