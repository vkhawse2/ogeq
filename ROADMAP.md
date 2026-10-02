# OGEQ -- Development roadmap

## Phase 1 -- Standalone DSP engine (START HERE)
- [x] Biquad filter (RBJ cookbook: peaking, low/high shelf, low/high pass)
- [x] Equalizer (10 bands, per-channel state, bypass)
- [x] DspProcessor (settings handoff, preamp)
- [ ] Unit tests (frequency response, clipping, performance)
- [ ] Standalone test player (play audio through DSP, verify audibly)

## Phase 2 -- Windows APO
- [ ] Fresh CLSID (never reuse MiniEQ's)
- [ ] COM aggregation (delegating IUnknown, FTM, IAgileObject)
- [ ] u32MaxInstances = 1 (24H2 graph-builder lesson)
- [ ] Initialize: endpoint GUID + settings load only
- [ ] LockForProcess: format negotiation (float32), DSP configure
- [ ] APOProcess: RT-safe processing, heartbeat via per-endpoint shm
- [ ] Static CRT (/MT)
- [ ] Registration: per-endpoint FxProperties, SFX-first on 24H2
- [ ] DisableProtectedAudioDG consent flow (unsigned APO)

## Phase 3 -- UI and service
- [ ] Native Win32 UI (tray + main window)
- [ ] Per-device EQ persistence (%APPDATA%\OGEQ\devices.ini)
- [ ] Attach/detach with one UAC per action
- [ ] Crash breaker (audiodg PID watch, latch on loop)
- [ ] Unified diagnostics dump

## Phase 4 -- Spatial audio
- [ ] Virtual speaker positioning, HRTF profiles
- [ ] Bypass for competitive games
- [ ] Never double-process with Windows Spatial Sound

## Phase 5 -- Distribution
- [ ] WiX MSI installer
- [ ] Driver/APO signing plan

## Lessons carried from MiniEQ (do not regress)
1. Unsigned APOs need DisableProtectedAudioDG=1 (with explicit user consent).
2. PKEY_Endpoint_Disable_SysFx crash-loop lockout must be checked/cleared.
3. Never do file I/O inside the APO in production builds.
4. Per-endpoint status channel (not one global mapping).
5. "Live" banner must be measured (heartbeat), not expected (build id).
6. Breaker: crash loop -> SAFE/DETACHED, latched until deliberate user action.
7. Install over the old version; never manual uninstall/reinstall churn.
8. Every handed-over build gets an incremented visible version.
9. Green CI is not runtime proof.
