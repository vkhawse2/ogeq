// OgeqUI.h -- main window for the OGEQ UI (Phase 3).
//
// Landscape layout (~900x500) for comfortable EQ slider spacing.
// Native Win32, no frameworks. Text never overlaps/crops/wraps.

#pragma once

#include <windows.h>
#include <commctrl.h>

// Control IDs
#define IDC_DEVICE_COMBO      101
#define IDC_ATTACH_BTN        102
#define IDC_DIAG_BTN          103
#define IDC_PRESET_COMBO      104
#define IDC_BYPASS_CHECK      105
#define IDC_XFEED_CHECK       106
#define IDC_BAND5_RADIO       107
#define IDC_BAND10_RADIO      108
#define IDC_EQ_SLIDER_BASE    200  // 200..209 for 10 bands
#define IDC_EQ_LABEL_BASE     300  // 300..309 band labels
#define IDC_EQ_VALUE_BASE     400  // 400..409 dB value labels
#define IDC_STATUS_HEARTBEAT  501
#define IDC_STATUS_PATH       502
#define IDC_STATUS_BREAKER    503
#define IDC_STATUS_BAR        504

#define OGEQ_MAX_BANDS 10
#define OGEQ_WINDOW_W 1000
#define OGEQ_WINDOW_H 520

// 5-band layout
static const float kBandFreqs5[5] = { 60, 230, 910, 3600, 14000 };
static const wchar_t* kBandLabels5[5] = { L"60 Hz", L"230 Hz", L"910 Hz", L"3.6 kHz", L"14 kHz" };

// 10-band layout (standard graphic EQ)
static const float kBandFreqs10[OGEQ_MAX_BANDS] = {
    31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000
};
static const wchar_t* kBandLabels10[OGEQ_MAX_BANDS] = {
    L"31", L"62", L"125", L"250", L"500",
    L"1k", L"2k", L"4k", L"8k", L"16k"
};

struct OgeqUIState {
    HWND hwndMain;
    HWND hwndDeviceCombo;
    HWND hwndAttachBtn;
    HWND hwndPresetCombo;
    HWND hwndBypassCheck;
    HWND hwndXfeedCheck;
    HWND hwndBand5Radio;
    HWND hwndBand10Radio;
    HWND hwndSliders[OGEQ_MAX_BANDS];
    HWND hwndFreqLabels[OGEQ_MAX_BANDS];
    HWND hwndValueLabels[OGEQ_MAX_BANDS];
    HWND hwndHeartbeat;
    HWND hwndPath;
    HWND hwndBreaker;
    HWND hwndStatusBar;

    float bandGains[OGEQ_MAX_BANDS];  // in dB, -12..+12
    int numBands;  // 5 or 10
    bool bypass;
    bool crossfeed;
    bool attached;
};

// Entry point
int OgeqUIRun(HINSTANCE hInstance, int nCmdShow);
