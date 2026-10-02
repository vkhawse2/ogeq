// OgeqHeartbeat.cpp -- console heartbeat monitor for OGEQ APO.
//
// Usage:
//   ogeq_heartbeat.exe --endpoint {guid} [--watch]
//
// Without --watch: reads once and prints status.
// With --watch: polls every 500ms, shows live heartbeat.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdio.h>
#include <string>
#include "../Shared/OgeqStatusChannel.h"
#include "../Shared/OgeqIpc.h"

#pragma comment(lib, "ogeq_shared.lib")

static void PrintStatus(const ogeq::OgeqStatus& s) {
    wprintf(L"  version:    %u\n", s.version);
    wprintf(L"  heartbeat:  %u (APOProcess calls)\n", s.heartbeat);
    wprintf(L"  lockCalls:  %u\n", s.lockCalls);
    wprintf(L"  lastLockHr: 0x%08X\n", (uint32_t)s.lastLockHr);
    wprintf(L"  channels:   %u\n", s.channels);
    wprintf(L"  sampleRate: %u Hz\n", s.sampleRate);
}

int wmain(int argc, wchar_t** argv) {
    std::wstring endpoint;
    bool watch = false;

    for (int i = 1; i < argc; i++) {
        std::wstring a = argv[i];
        if (a == L"--endpoint" && i + 1 < argc) endpoint = argv[++i];
        else if (a == L"--watch") watch = true;
        else {
            wprintf(L"Usage: ogeq_heartbeat --endpoint {guid} [--watch]\n");
            return 1;
        }
    }

    if (endpoint.empty()) {
        wprintf(L"Usage: ogeq_heartbeat --endpoint {guid} [--watch]\n");
        return 1;
    }

    ogeq::StatusChannelReader reader;
    if (!reader.open(endpoint.c_str())) {
        wprintf(L"No status channel for this endpoint.\n");
        wprintf(L"The APO hasn't created it yet (not loaded, or wrong endpoint).\n");
        return 2;
    }

    if (!watch) {
        ogeq::OgeqStatus s{};
        if (reader.read(s)) {
            wprintf(L"OGEQ status for %s:\n", endpoint.c_str());
            PrintStatus(s);
        } else {
            wprintf(L"Channel exists but read failed (torn or empty).\n");
        }
        return 0;
    }

    // Watch mode
    wprintf(L"Watching heartbeat for %s (Ctrl+C to stop)...\n", endpoint.c_str());
    uint32_t lastHb = 0;
    bool first = true;
    while (true) {
        ogeq::OgeqStatus s{};
        if (reader.read(s)) {
            if (first) {
                wprintf(L"  lockCalls=%u lastLockHr=0x%08X ch=%u sr=%u\n",
                        s.lockCalls, (uint32_t)s.lastLockHr, s.channels, s.sampleRate);
                first = false;
            }
            uint32_t delta = s.heartbeat - lastHb;
            const wchar_t* state = (delta > 0) ? L"LIVE" : L"STALLED";
            wprintf(L"\r  heartbeat=%u (+%u) [%s]   ", s.heartbeat, delta, state);
            fflush(stdout);
            lastHb = s.heartbeat;
        } else {
            wprintf(L"\r  read failed...   ");
            fflush(stdout);
        }
        Sleep(500);
    }
    return 0;
}
