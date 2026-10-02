// OgeqIpc.h -- UI <-> APO shared-memory contract.
//
// Versioned. The APO writes; the UI reads. Seqlock for torn-read protection.
// The APO never blocks on the UI; the UI never writes into this mapping.

#pragma once

#include <cstdint>

namespace ogeq {

constexpr uint32_t kIpcVersion = 1;

// Name: L"Global\\OGEQ_Status_<endpoint-hash>" -- per-endpoint, so multiple
// devices don't share one mapping (MiniEQ lesson: one global mapping with
// many writers was a crash suspect).
struct OgeqStatus {
    uint32_t version;        // kIpcVersion
    uint32_t seqBegin;       // seqlock
    uint32_t heartbeat;      // increments per APOProcess
    uint32_t lockCalls;      // LockForProcess count
    int32_t  lastLockHr;     // last LockForProcess HRESULT
    uint32_t channels;
    uint32_t sampleRate;
    uint32_t seqEnd;         // seqlock
};

inline const wchar_t* IpcMappingPrefix() { return L"Global\\OGEQ_Status_"; }

} // namespace ogeq
