// WavFile.h -- minimal WAV reader/writer for the OGEQ test player.
//
// Supports: 16-bit PCM and 32-bit float, mono/stereo, any sample rate.
// No external dependencies. Not for production use -- just the test harness.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ogeq {

struct WavData {
    std::vector<float> samples; // interleaved float32, -1..1
    uint32_t sampleRate = 0;
    uint16_t channels = 0;
};

// Returns empty WavData (channels==0) on failure; error in errMsg.
WavData readWav(const std::string& path, std::string& errMsg);

// Writes 16-bit PCM. Returns false on failure.
bool writeWav16(const std::string& path, const WavData& wav, std::string& errMsg);

} // namespace ogeq
