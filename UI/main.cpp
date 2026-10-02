// main.cpp -- OGEQ UI entry point (native Win32, low RAM).
//
// The UI never touches audio. It:
//  - lists endpoints, attaches/detaches the APO (one UAC per action)
//  - edits per-device EQ, persists to %APPDATA%\\OGEQ\\devices.ini
//  - reads the APO heartbeat via the shared-memory status channel
//  - runs the crash-breaker (audiodg PID watch; latch on crash loop)
//
// Text must never overlap/crop/wrap poorly (standing UI standard).

#include <windows.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // TODO(Phase 3): build the main window, tray icon, checklist dialog.
    MessageBoxW(nullptr, L"OGEQ UI -- Phase 3 not yet implemented.",
                L"OGEQ", MB_OK | MB_ICONINFORMATION);
    return 0;
}
