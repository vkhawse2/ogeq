// main.cpp -- OGEQ test player: apply DSP to a WAV file, hear the result.
//
// Usage:
//   ogeq_player input.wav output.wav --eq "freq,q,gain;freq,q,gain;..."
//   ogeq_player input.wav output.wav --preset bass_boost
//
// Examples:
//   ogeq_player song.wav song_eq.wav --eq "60,1.0,+6;1000,1.0,0;14000,1.0,+3"
//   ogeq_player song.wav song_bb.wav --preset bass_boost
//
// Presets: flat, bass_boost, treble_boost, vocal_boost, loudness
//
// This is the Phase 1 "hear it before it touches Windows" tool.
// It uses only the DSP library -- no Windows audio, no APO, no drivers.

#include "WavFile.h"
#include "DspProcessor.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <map>

namespace {

void usage() {
    std::printf(
        "OGEQ test player -- hear the DSP before it touches Windows.\n"
        "\n"
        "Usage:\n"
        "  ogeq_player input.wav output.wav --eq \"freq,q,gain;...\"\n"
        "  ogeq_player input.wav output.wav --preset <name>\n"
        "\n"
        "Presets: flat, bass_boost, treble_boost, vocal_boost, loudness\n"
        "\n"
        "Example:\n"
        "  ogeq_player song.wav song_eq.wav --eq \"60,1.0,+6;230,1.0,+2;14000,1.0,+3\"\n");
}

bool parseEq(const std::string& spec, ogeq::DspSettings& settings) {
    // Format: "freq,q,gain;freq,q,gain;..." (freq in Hz, q dimensionless, gain in dB)
    size_t pos = 0;
    int band = 0;
    while (pos < spec.size() && band < ogeq::kMaxBands) {
        size_t end = spec.find(';', pos);
        std::string token = spec.substr(pos, end == std::string::npos ? end : end - pos);
        float freq, q, gain;
        if (std::sscanf(token.c_str(), "%f,%f,%f", &freq, &q, &gain) != 3) {
            std::printf("Bad band spec: '%s' (want freq,q,gain)\n", token.c_str());
            return false;
        }
        ogeq::EqBand b;
        b.type = ogeq::FilterType::Peaking;
        b.freqHz = freq; b.q = q; b.gainDb = gain; b.enabled = true;
        settings.bands[band++] = b;
        if (end == std::string::npos) break;
        pos = end + 1;
    }
    settings.numBands = band;
    return band > 0;
}

bool applyPreset(const std::string& name, ogeq::DspSettings& s) {
    // 5-band presets: {freq, q, gain}
    static const std::map<std::string, std::vector<std::tuple<float,float,float>>> presets = {
        {"flat",         {{60.0f,1.0f,0.0f},  {230.0f,1.0f,0.0f},  {910.0f,1.0f,0.0f},  {3600.0f,1.0f,0.0f},  {14000.0f,1.0f,0.0f}}},
        {"bass_boost",   {{60.0f,1.0f,6.0f},  {230.0f,1.0f,3.0f},  {910.0f,1.0f,0.0f},  {3600.0f,1.0f,0.0f},  {14000.0f,1.0f,0.0f}}},
        {"treble_boost", {{60.0f,1.0f,0.0f},  {230.0f,1.0f,0.0f},  {910.0f,1.0f,0.0f},  {3600.0f,1.0f,2.0f},  {14000.0f,1.0f,5.0f}}},
        {"vocal_boost",  {{60.0f,1.0f,-2.0f}, {230.0f,1.0f,0.0f},  {910.0f,1.0f,3.0f},  {3600.0f,1.0f,4.0f},  {14000.0f,1.0f,1.0f}}},
        {"loudness",     {{60.0f,1.0f,5.0f},  {230.0f,1.0f,2.0f},  {910.0f,1.0f,0.0f},  {3600.0f,1.0f,1.0f},  {14000.0f,1.0f,4.0f}}},
    };
    auto it = presets.find(name);
    if (it == presets.end()) {
        std::printf("Unknown preset '%s'. Available:", name.c_str());
        for (auto& p : presets) std::printf(" %s", p.first.c_str());
        std::printf("\n");
        return false;
    }
    int i = 0;
    for (auto& [freq, q, gain] : it->second) {
        ogeq::EqBand b;
        b.type = ogeq::FilterType::Peaking;
        b.freqHz = freq; b.q = q; b.gainDb = gain; b.enabled = true;
        s.bands[i++] = b;
    }
    s.numBands = i;
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 5) { usage(); return 1; }

    std::string inPath = argv[1];
    std::string outPath = argv[2];
    std::string mode = argv[3];
    std::string spec = argv[4];

    ogeq::DspSettings settings;

    if (mode == "--eq") {
        if (!parseEq(spec, settings)) return 1;
    } else if (mode == "--preset") {
        if (!applyPreset(spec, settings)) return 1;
    } else {
        usage(); return 1;
    }

    std::string err;
    ogeq::WavData wav = ogeq::readWav(inPath, err);
    if (wav.channels == 0) {
        std::printf("Read failed: %s\n", err.c_str());
        return 1;
    }
    std::printf("Input: %u Hz, %u ch, %.1f sec\n",
                wav.sampleRate, wav.channels,
                (double)wav.samples.size() / wav.channels / wav.sampleRate);

    ogeq::DspProcessor dsp;
    dsp.applySettings(settings);
    dsp.configure((float)wav.sampleRate, wav.channels);

    // Process in 10ms chunks (like the real APO would).
    size_t framesTotal = wav.samples.size() / wav.channels;
    size_t chunkFrames = wav.sampleRate / 100;
    std::vector<float> out(wav.samples.size());

    for (size_t off = 0; off < framesTotal; off += chunkFrames) {
        size_t n = std::min(chunkFrames, framesTotal - off);
        dsp.process(wav.samples.data() + off * wav.channels,
                    out.data() + off * wav.channels,
                    (uint32_t)n);
    }

    ogeq::WavData outWav;
    outWav.samples = std::move(out);
    outWav.sampleRate = wav.sampleRate;
    outWav.channels = wav.channels;

    if (!ogeq::writeWav16(outPath, outWav, err)) {
        std::printf("Write failed: %s\n", err.c_str());
        return 1;
    }
    std::printf("Wrote: %s (%d bands applied)\n", outPath.c_str(), settings.numBands);
    return 0;
}
