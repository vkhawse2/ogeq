// OgeqUI.cpp -- landscape Win32 UI for OGEQ.
//
// Layout (900x500, landscape):
//   Row 1: Title + device combo + Attach + Diagnostics buttons
//   Row 2: Preset combo + Bypass + Crossfeed checkboxes
//   Row 3: EQ sliders (5 bands, vertical) + status panel (right side)
//   Row 4: Status bar (heartbeat, version)
//
// The UI never touches audio. It writes settings to %APPDATA%\\OGEQ\\devices.ini
// and reads the APO heartbeat via the per-endpoint shared-memory channel.

#include "OgeqUI.h"
#include <windowsx.h>
#include <stdio.h>

static OgeqUIState g_state = {};
static HINSTANCE g_hInst = nullptr;

// Slider range: -120 to +120 (tenths of dB, so -12.0 to +12.0 dB)
#define SLIDER_MIN -120
#define SLIDER_MAX  120

static void UpdateValueLabel(int band) {
    wchar_t buf[32];
    float db = g_state.bandGains[band];
    swprintf_s(buf, L"%+.1f dB", db);
    SetWindowTextW(g_state.hwndValueLabels[band], buf);
}

static void UpdateStatusBar() {
    wchar_t buf[128];
    swprintf_s(buf, L"OGEQ v%s | Build %s",
#ifdef OGEQ_VERSION
        L"" OGEQ_VERSION,
#else
        L"0.1.0",
#endif
#ifdef OGEQ_BUILD_ID
        L"" OGEQ_BUILD_ID
#else
        L"dev"
#endif
    );
    // Status bar simple text (first part)
    SendMessageW(g_state.hwndStatusBar, SB_SETTEXTW, 0, (LPARAM)buf);
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // --- Row 1: Device selection ---
        CreateWindowW(L"STATIC", L"Device:", WS_VISIBLE | WS_CHILD,
                      16, 16, 60, 24, hwnd, nullptr, g_hInst, nullptr);
        g_state.hwndDeviceCombo = CreateWindowW(WC_COMBOBOXW,
            L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
            80, 12, 320, 200, hwnd, (HMENU)IDC_DEVICE_COMBO, g_hInst, nullptr);
        // TODO: enumerate MMDevice endpoints
        ComboBox_AddString(g_state.hwndDeviceCombo, L"Headphones (MH139)");
        ComboBox_AddString(g_state.hwndDeviceCombo, L"Headphones (Airdopes 411ANC)");
        ComboBox_SetCurSel(g_state.hwndDeviceCombo, 0);

        g_state.hwndAttachBtn = CreateWindowW(L"BUTTON", L"Attach",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            420, 12, 100, 28, hwnd, (HMENU)IDC_ATTACH_BTN, g_hInst, nullptr);

        CreateWindowW(L"BUTTON", L"Diagnostics",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            530, 12, 110, 28, hwnd, (HMENU)IDC_DIAG_BTN, g_hInst, nullptr);

        // Title on the right
        CreateWindowW(L"STATIC", L"OGEQ",
            WS_VISIBLE | WS_CHILD | SS_RIGHT,
            700, 8, 184, 36, hwnd, nullptr, g_hInst, nullptr);

        // --- Row 2: Preset + toggles ---
        CreateWindowW(L"STATIC", L"Preset:", WS_VISIBLE | WS_CHILD,
                      16, 56, 60, 24, hwnd, nullptr, g_hInst, nullptr);
        g_state.hwndPresetCombo = CreateWindowW(WC_COMBOBOXW,
            L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
            80, 52, 200, 200, hwnd, (HMENU)IDC_PRESET_COMBO, g_hInst, nullptr);
        ComboBox_AddString(g_state.hwndPresetCombo, L"Flat");
        ComboBox_AddString(g_state.hwndPresetCombo, L"Bass Boost");
        ComboBox_AddString(g_state.hwndPresetCombo, L"Treble Boost");
        ComboBox_AddString(g_state.hwndPresetCombo, L"Vocal Boost");
        ComboBox_AddString(g_state.hwndPresetCombo, L"Loudness");
        ComboBox_AddString(g_state.hwndPresetCombo, L"Custom");
        ComboBox_SetCurSel(g_state.hwndPresetCombo, 0);

        g_state.hwndBypassCheck = CreateWindowW(L"BUTTON", L"Bypass EQ",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            300, 54, 120, 24, hwnd, (HMENU)IDC_BYPASS_CHECK, g_hInst, nullptr);

        g_state.hwndXfeedCheck = CreateWindowW(L"BUTTON", L"Crossfeed",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            430, 54, 120, 24, hwnd, (HMENU)IDC_XFEED_CHECK, g_hInst, nullptr);

        // --- Row 3: EQ sliders (left, wide) + Status panel (right) ---
        int sliderStartX = 40;
        int sliderSpacing = 110;
        int sliderTop = 110;
        int sliderHeight = 280;

        for (int i = 0; i < OGEQ_NUM_BANDS; i++) {
            int x = sliderStartX + i * sliderSpacing;

            // Frequency label above slider
            CreateWindowW(L"STATIC", kBandLabels[i],
                WS_VISIBLE | WS_CHILD | SS_CENTER,
                x - 30, sliderTop - 24, 110, 20, hwnd, nullptr, g_hInst, nullptr);

            // Vertical slider (TBS_VERT, TBS_BOTH because we want - at bottom)
            g_state.hwndSliders[i] = CreateWindowW(TRACKBAR_CLASSW, L"",
                WS_VISIBLE | WS_CHILD | TBS_VERT | TBS_BOTH | TBS_NOTICKS,
                x, sliderTop, 50, sliderHeight,
                hwnd, (HMENU)(IDC_EQ_SLIDER_BASE + i), g_hInst, nullptr);
            SendMessageW(g_state.hwndSliders[i], TBM_SETRANGE, TRUE,
                         MAKELONG(SLIDER_MIN, SLIDER_MAX));
            SendMessageW(g_state.hwndSliders[i], TBM_SETPOS, TRUE, 0);
            // Invert: up = positive. TBS_DOWNISLEFT not set; we handle mapping.
            // Actually for vertical: top = max. We want top = +12.
            // TBM_SETPOS with our range: SLIDER_MAX at top.

            // dB value label below slider
            g_state.hwndValueLabels[i] = CreateWindowW(L"STATIC", L"+0.0 dB",
                WS_VISIBLE | WS_CHILD | SS_CENTER,
                x - 30, sliderTop + sliderHeight + 8, 110, 20,
                hwnd, (HMENU)(IDC_EQ_VALUE_BASE + i), g_hInst, nullptr);

            g_state.bandGains[i] = 0.0f;
        }

        // Status panel (right side)
        int panelX = 640;
        CreateWindowW(L"BUTTON", L"Status",
            WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
            panelX, sliderTop - 30, 230, 200, hwnd, nullptr, g_hInst, nullptr);

        CreateWindowW(L"STATIC", L"Heartbeat:", WS_VISIBLE | WS_CHILD,
                      panelX + 16, sliderTop, 90, 20, hwnd, nullptr, g_hInst, nullptr);
        g_state.hwndHeartbeat = CreateWindowW(L"STATIC", L"--",
            WS_VISIBLE | WS_CHILD | SS_LEFT,
            panelX + 110, sliderTop, 100, 20, hwnd, (HMENU)IDC_STATUS_HEARTBEAT,
            g_hInst, nullptr);

        CreateWindowW(L"STATIC", L"Path:", WS_VISIBLE | WS_CHILD,
                      panelX + 16, sliderTop + 30, 90, 20, hwnd, nullptr, g_hInst, nullptr);
        g_state.hwndPath = CreateWindowW(L"STATIC", L"Detached",
            WS_VISIBLE | WS_CHILD | SS_LEFT,
            panelX + 110, sliderTop + 30, 100, 20, hwnd, (HMENU)IDC_STATUS_PATH,
            g_hInst, nullptr);

        CreateWindowW(L"STATIC", L"Breaker:", WS_VISIBLE | WS_CHILD,
                      panelX + 16, sliderTop + 60, 90, 20, hwnd, nullptr, g_hInst, nullptr);
        g_state.hwndBreaker = CreateWindowW(L"STATIC", L"OK",
            WS_VISIBLE | WS_CHILD | SS_LEFT,
            panelX + 110, sliderTop + 60, 100, 20, hwnd, (HMENU)IDC_STATUS_BREAKER,
            g_hInst, nullptr);

        // --- Row 4: Status bar ---
        g_state.hwndStatusBar = CreateWindowW(STATUSCLASSNAMEW, L"",
            WS_VISIBLE | WS_CHILD | SBARS_SIZEGRIP,
            0, 0, 0, 0, hwnd, (HMENU)IDC_STATUS_BAR, g_hInst, nullptr);
        int parts[] = { 400, -1 };
        SendMessageW(g_state.hwndStatusBar, SB_SETPARTS, 2, (LPARAM)parts);
        UpdateStatusBar();
        SendMessageW(g_state.hwndStatusBar, SB_SETTEXTW, 1,
                     (LPARAM)L"Select a device and click Attach");

        // Timer for heartbeat polling (500 ms)
        SetTimer(hwnd, 1, 500, nullptr);
        break;
    }

    case WM_HSCROLL:
    case WM_VSCROLL: {
        // EQ slider moved
        HWND hwndSlider = (HWND)lParam;
        for (int i = 0; i < OGEQ_NUM_BANDS; i++) {
            if (hwndSlider == g_state.hwndSliders[i]) {
                int pos = (int)SendMessageW(hwndSlider, TBM_GETPOS, 0, 0);
                // Vertical slider: top = max (SLIDER_MAX = +120 = +12.0 dB)
                g_state.bandGains[i] = (float)pos / 10.0f;
                UpdateValueLabel(i);
                // TODO: push to APO via shared memory / service
                // TODO: mark preset as "Custom"
                break;
            }
        }
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);
        if (id == IDC_PRESET_COMBO && code == CBN_SELCHANGE) {
            int sel = ComboBox_GetCurSel(g_state.hwndPresetCombo);
            // Preset gains: {60, 230, 910, 3600, 14000}
            static const float presets[][OGEQ_NUM_BANDS] = {
                { 0, 0, 0, 0, 0 },       // Flat
                { 6, 3, 0, 0, 0 },       // Bass Boost
                { 0, 0, 0, 2, 5 },       // Treble Boost
                { -2, 0, 3, 4, 1 },      // Vocal Boost
                { 5, 2, 0, 1, 4 },       // Loudness
                { 0, 0, 0, 0, 0 },       // Custom (keep current)
            };
            if (sel >= 0 && sel < 5) {
                for (int i = 0; i < OGEQ_NUM_BANDS; i++) {
                    g_state.bandGains[i] = presets[sel][i];
                    SendMessageW(g_state.hwndSliders[i], TBM_SETPOS, TRUE,
                                 (int)(presets[sel][i] * 10));
                    UpdateValueLabel(i);
                }
            }
        }
        else if (id == IDC_BYPASS_CHECK) {
            g_state.bypass = Button_GetCheck(g_state.hwndBypassCheck) == BST_CHECKED;
            // TODO: push to APO
        }
        else if (id == IDC_XFEED_CHECK) {
            g_state.crossfeed = Button_GetCheck(g_state.hwndXfeedCheck) == BST_CHECKED;
            // TODO: push to APO
        }
        else if (id == IDC_ATTACH_BTN) {
            // TODO(Phase 3): elevated attach/detach flow
            MessageBoxW(hwnd, L"Attach/detach coming in Phase 3.\nThe APO must be registered first (regsvr32).",
                        L"OGEQ", MB_OK | MB_ICONINFORMATION);
        }
        else if (id == IDC_DIAG_BTN) {
            // TODO(Phase 3): unified diagnostics
            MessageBoxW(hwnd, L"Diagnostics coming in Phase 3.", L"OGEQ", MB_OK | MB_ICONINFORMATION);
        }
        break;
    }

    case WM_TIMER: {
        // TODO(Phase 3): read heartbeat from per-endpoint status channel
        // For now, show placeholder
        // SetWindowTextW(g_state.hwndHeartbeat, L"...");

        // TODO: update breaker status from audiodg PID watch
        break;
    }

    case WM_SIZE: {
        // Keep status bar at bottom
        SendMessageW(g_state.hwndStatusBar, WM_SIZE, 0, 0);
        break;
    }

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int OgeqUIRun(HINSTANCE hInstance, int nCmdShow) {
    g_hInst = hInstance;
    InitCommonControls();

    const wchar_t* className = L"OGEQ_MainWindow";
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = className;
    RegisterClassExW(&wc);

    // Landscape: wider than tall
    HWND hwnd = CreateWindowExW(0, className, L"OGEQ — System Equalizer",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, OGEQ_WINDOW_W, OGEQ_WINDOW_H,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return 1;

    g_state.hwndMain = hwnd;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
