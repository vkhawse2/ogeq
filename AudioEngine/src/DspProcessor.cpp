// DspProcessor.cpp

#include "DspProcessor.h"
#include <cmath>

namespace ogeq {

DspProcessor::DspProcessor() = default;

void DspProcessor::applySettings(const DspSettings& settings) {
    // Control thread: stage into pending. The RT thread picks it up at the
    // next process() call boundary. Single-producer/single-consumer; the APO
    // calls applySettings from its non-RT threads only.
    pending_ = settings;
    pendingDirty_ = true;
}

void DspProcessor::configure(float sampleRate, int channels) {
    eq_.configure(sampleRate, channels, pending_.numBands > 0 ? pending_.numBands : 5);
}

void DspProcessor::process(const float* in, float* out, uint32_t frames) noexcept {
    if (pendingDirty_) {
        // Not strictly lock-free against a concurrent applySettings, but the
        // APO only calls applySettings from Initialize/LockForProcess paths,
        // never concurrently with APOProcess. Documented contract.
        pendingDirty_ = false;
        eq_.setPreampDb(pending_.preampDb);
        eq_.setBypass(pending_.eqBypass);
        for (int i = 0; i < pending_.numBands && i < kMaxBands; ++i)
            eq_.setBand(i, pending_.bands[i]);
    }
    eq_.process(in, out, frames);
}

} // namespace ogeq
