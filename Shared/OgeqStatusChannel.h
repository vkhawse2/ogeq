// OgeqStatusChannel.h -- per-endpoint heartbeat via shared memory.
//
// The APO writes; the UI reads. Each endpoint gets its OWN mapping
// (MiniEQ lesson: one global mapping with multiple writers was a crash
// suspect). Name: L"Global\\OGEQ_Status_<8-hex-endpoint-hash>".
//
// Seqlock for torn-read protection. The APO never blocks on the UI.

#pragma once

#include "OgeqIpc.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace ogeq {

class StatusChannelWriter {
public:
    StatusChannelWriter() = default;
    ~StatusChannelWriter() { close(); }

    // Called from LockForProcess (not RT). endpointId is the MMDevice id string.
    bool open(const wchar_t* endpointId);

    // Called from APOProcess (RT-safe: just a sequence bump + counter).
    void heartbeat() noexcept;

    // Called from LockForProcess to publish format info.
    void publishFormat(uint32_t channels, uint32_t sampleRate, int32_t lockHr) noexcept;

    void close() noexcept;

    bool isOpen() const { return view_ != nullptr; }

private:
    static void hashEndpoint(const wchar_t* endpointId, wchar_t* outName, size_t outChars);

#ifdef _WIN32
    HANDLE mapping_ = nullptr;
    OgeqStatus* view_ = nullptr;
#endif
};

class StatusChannelReader {
public:
    StatusChannelReader() = default;
    ~StatusChannelReader() { close(); }

    bool open(const wchar_t* endpointId);
    void close() noexcept;

    // Returns false if the channel doesn't exist or data is torn.
    bool read(OgeqStatus& out) const noexcept;

    bool isOpen() const { return view_ != nullptr; }

private:
#ifdef _WIN32
    HANDLE mapping_ = nullptr;
    const OgeqStatus* view_ = nullptr;
#endif
};

} // namespace ogeq
