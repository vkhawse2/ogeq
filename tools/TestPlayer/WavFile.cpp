// WavFile.cpp

#include "WavFile.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace ogeq {

#pragma pack(push, 1)
struct RiffHeader { char riff[4]; uint32_t size; char wave[4]; };
struct FmtChunk {
    char id[4]; uint32_t size; uint16_t format; uint16_t channels;
    uint32_t sampleRate; uint32_t byteRate; uint16_t blockAlign; uint16_t bits;
};
struct ChunkHeader { char id[4]; uint32_t size; };
#pragma pack(pop)

WavData readWav(const std::string& path, std::string& errMsg) {
    WavData wav;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { errMsg = "cannot open file"; return wav; }

    RiffHeader rh;
    if (std::fread(&rh, sizeof(rh), 1, f) != 1 ||
        std::memcmp(rh.riff, "RIFF", 4) || std::memcmp(rh.wave, "WAVE", 4)) {
        errMsg = "not a WAV file"; std::fclose(f); return wav;
    }

    FmtChunk fmt{};
    bool haveFmt = false, haveData = false;
    std::vector<uint8_t> rawData;

    while (!haveData) {
        ChunkHeader ch;
        if (std::fread(&ch, sizeof(ch), 1, f) != 1) break;
        if (!std::memcmp(ch.id, "fmt ", 4)) {
            uint32_t toRead = std::min(ch.size, (uint32_t)sizeof(FmtChunk) - 8);
            std::fread(((uint8_t*)&fmt) + 8, 1, toRead, f);
            std::memcpy(fmt.id, "fmt ", 4); fmt.size = ch.size;
            if (ch.size > toRead) std::fseek(f, ch.size - toRead, SEEK_CUR);
            haveFmt = true;
        } else if (!std::memcmp(ch.id, "data", 4)) {
            rawData.resize(ch.size);
            if (std::fread(rawData.data(), 1, ch.size, f) != ch.size) {
                errMsg = "truncated data"; std::fclose(f); return wav;
            }
            haveData = true;
        } else {
            std::fseek(f, ch.size, SEEK_CUR);
        }
    }
    std::fclose(f);

    if (!haveFmt || !haveData) { errMsg = "missing fmt/data chunk"; return wav; }
    if (fmt.format != 1 && fmt.format != 3) { errMsg = "only PCM16 and float32 supported"; return wav; }
    if (fmt.bits != 16 && fmt.bits != 32) { errMsg = "only 16/32-bit supported"; return wav; }

    size_t bytesPerSample = fmt.bits / 8;
    size_t nSamples = rawData.size() / bytesPerSample;
    wav.samples.resize(nSamples);
    wav.sampleRate = fmt.sampleRate;
    wav.channels = fmt.channels;

    if (fmt.format == 1) { // int16
        const int16_t* p = reinterpret_cast<const int16_t*>(rawData.data());
        for (size_t i = 0; i < nSamples; ++i)
            wav.samples[i] = (float)p[i] / 32768.0f;
    } else { // float32
        const float* p = reinterpret_cast<const float*>(rawData.data());
        for (size_t i = 0; i < nSamples; ++i)
            wav.samples[i] = p[i];
    }
    return wav;
}

bool writeWav16(const std::string& path, const WavData& wav, std::string& errMsg) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { errMsg = "cannot open output"; return false; }

    uint32_t dataBytes = (uint32_t)(wav.samples.size() * 2);
    uint32_t riffSize = 36 + dataBytes;

    RiffHeader rh;
    std::memcpy(rh.riff, "RIFF", 4); rh.size = riffSize; std::memcpy(rh.wave, "WAVE", 4);
    std::fwrite(&rh, sizeof(rh), 1, f);

    FmtChunk fmt;
    std::memcpy(fmt.id, "fmt ", 4); fmt.size = 16; fmt.format = 1;
    fmt.channels = wav.channels; fmt.sampleRate = wav.sampleRate;
    fmt.bits = 16;
    fmt.blockAlign = wav.channels * 2;
    fmt.byteRate = wav.sampleRate * fmt.blockAlign;
    std::fwrite(&fmt, sizeof(fmt), 1, f);

    ChunkHeader dh;
    std::memcpy(dh.id, "data", 4); dh.size = dataBytes;
    std::fwrite(&dh, sizeof(dh), 1, f);

    for (float s : wav.samples) {
        float clamped = std::max(-1.0f, std::min(1.0f, s));
        int16_t v = (int16_t)std::lround(clamped * 32767.0f);
        std::fwrite(&v, 2, 1, f);
    }
    std::fclose(f);
    return true;
}

} // namespace ogeq
