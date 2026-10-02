// OgeqStatusChannel.cpp

#include "OgeqStatusChannel.h"
#include <cstdio>
#include <cwchar>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ogeq {

void StatusChannelWriter::hashEndpoint(const wchar_t* endpointId, wchar_t* outName, size_t outChars) {
    // FNV-1a 32-bit hash of the endpoint string.
    uint32_t h = 2166136261u;
    for (const wchar_t* p = endpointId; *p; ++p) {
        uint32_t ch = (uint32_t)*p;
        h ^= (ch & 0xFFFF);
        h *= 16777619u;
        h ^= ((ch >> 16) & 0xFFFF);
        h *= 16777619u;
    }
#ifdef _WIN32
    std::swprintf(outName, outChars, L"%s%08x", IpcMappingPrefix(), h);
#else
    snprintf(nullptr, 0, ""); // non-Windows: stub
#endif
}

bool StatusChannelWriter::open(const wchar_t* endpointId) {
#ifdef _WIN32
    if (isOpen()) close();

    wchar_t name[64];
    hashEndpoint(endpointId, name, 64);

    mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                  0, sizeof(OgeqStatus), name);
    if (!mapping_) return false;

    view_ = (OgeqStatus*)MapViewOfFile(mapping_, FILE_MAP_WRITE, 0, 0, sizeof(OgeqStatus));
    if (!view_) { CloseHandle(mapping_); mapping_ = nullptr; return false; }

    // Initialize (first writer wins; adopted mappings keep existing data).
    bool isNew = (GetLastError() != ERROR_ALREADY_EXISTS);
    if (isNew) {
        view_->version = kIpcVersion;
        view_->seqBegin = view_->seqEnd = 0;
        view_->heartbeat = 0;
        view_->lockCalls = 0;
        view_->lastLockHr = 0;
        view_->channels = 0;
        view_->sampleRate = 0;
    }
    return true;
#else
    (void)endpointId;
    return false;
#endif
}

void StatusChannelWriter::heartbeat() noexcept {
#ifdef _WIN32
    if (!view_) return;
    // Seqlock write: bump begin, write data, bump end.
    // Single writer (the APO); readers retry on odd sequence.
    view_->seqBegin++;
#ifdef _M_IX86
    _ReadWriteBarrier();
#else
    MemoryBarrier();
#endif
    view_->heartbeat++;
#ifdef _M_IX86
    _ReadWriteBarrier();
#else
    MemoryBarrier();
#endif
    view_->seqEnd++;
#endif
}

void StatusChannelWriter::publishFormat(uint32_t channels, uint32_t sampleRate, int32_t lockHr) noexcept {
#ifdef _WIN32
    if (!view_) return;
    view_->seqBegin++;
#ifdef _M_IX86
    _ReadWriteBarrier();
#else
    MemoryBarrier();
#endif
    view_->channels = channels;
    view_->sampleRate = sampleRate;
    view_->lastLockHr = lockHr;
    view_->lockCalls++;
#ifdef _M_IX86
    _ReadWriteBarrier();
#else
    MemoryBarrier();
#endif
    view_->seqEnd++;
#endif
}

void StatusChannelWriter::close() noexcept {
#ifdef _WIN32
    if (view_) { UnmapViewOfFile(view_); view_ = nullptr; }
    if (mapping_) { CloseHandle(mapping_); mapping_ = nullptr; }
#endif
}

bool StatusChannelReader::open(const wchar_t* endpointId) {
#ifdef _WIN32
    if (isOpen()) close();

    wchar_t name[64];
    StatusChannelWriter::hashEndpoint(endpointId, name, 64);

    mapping_ = OpenFileMappingW(FILE_MAP_READ, FALSE, name);
    if (!mapping_) return false;

    view_ = (const OgeqStatus*)MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, sizeof(OgeqStatus));
    if (!view_) { CloseHandle(mapping_); mapping_ = nullptr; return false; }
    return true;
#else
    (void)endpointId;
    return false;
#endif
}

bool StatusChannelReader::read(OgeqStatus& out) const noexcept {
#ifdef _WIN32
    if (!view_) return false;
    // Seqlock read: sample begin, copy, sample end; retry if changed or odd.
    for (int tries = 0; tries < 3; ++tries) {
        uint32_t b = view_->seqBegin;
#ifdef _M_IX86
        _ReadBarrier();
#else
        MemoryBarrier();
#endif
        OgeqStatus tmp = *view_;
#ifdef _M_IX86
        _ReadBarrier();
#else
        MemoryBarrier();
#endif
        uint32_t e = view_->seqEnd;
        if (b == e && (b & 1) == 0 && tmp.version == kIpcVersion) {
            out = tmp;
            return true;
        }
    }
    return false;
#else
    (void)out;
    return false;
#endif
}

void StatusChannelReader::close() noexcept {
#ifdef _WIN32
    if (view_) { UnmapViewOfFile(view_); view_ = nullptr; }
    if (mapping_) { CloseHandle(mapping_); mapping_ = nullptr; }
#endif
}

} // namespace ogeq
