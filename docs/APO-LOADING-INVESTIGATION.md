# OGEQ APO Loading Investigation — 2026-10-02/03

## Summary

The OGEQ Audio Processing Object (system-wide EQ) builds correctly and registers
correctly, but the Windows audio engine (`audiodg.exe`) refuses to load it on the
test machine (Windows 11 24H2, build 26200). A bare-minimum test APO also fails to
load, proving the issue is systemic — not in OGEQ's code.

## What was verified working

- DLL builds cleanly with WDK 10.0.26100 (static CRT, `/MT`)
- All COM exports present: `DllGetClassObject`, `DllCanUnloadNow`,
  `DllRegisterServer`, `DllUnregisterServer`
- `regsvr32` succeeds; `CoCreateInstance` works from a test app
- COM registration correct: `HKCR\CLSID\{...}\InprocServer32` →
  `C:\Program Files\OGEQ\OGEQ_APO_trace.dll`, `ThreadingModel=Both`
- APO declaration complete in
  `HKLM\...\Audio\AudioEngine\AudioProcessingObjects\{...}`:
  `FriendlyName`, `Copyright`, `MajorVersion`/`MinorVersion`, `Flags`,
  `NumAPOInterfaces=2`, `APOInterface0` (IAudioProcessingObject),
  `APOInterface1` (IAudioSystemEffects), `u32MaxInstances=1`,
  connection counts = 1
- `IAudioSystemEffects` implemented (required per Microsoft docs:
  "the interface that makes the audio engine recognize a DLL as a
  systems effects APO")
- Full COM aggregation support in the class factory
- Endpoint `FxProperties` contains our CLSID in SFX slot 5
  (and Win11 chain slot 13) on both MH139 (USB) and Realtek speakers
- `DisableProtectedAudioDG=1` set and verified after reboot
- DLL staged in `C:\Program Files\OGEQ\` (accessible to LocalService)

## What was tried

1. SFX slot 5, EFX slot 7, Win11 chain slots 13/14/15 (REG_MULTI_SZ)
2. Single slot vs. multiple slots
3. MH139 USB endpoint and Realtek built-in speakers
4. Device unplug/replug, full reboot
5. Fresh CLSID to rule out engine-side blacklisting
6. Minimal bare-bones APO (pass-through, no DSP) with its own CLSID
7. INF-based installation (blocked: "INF does not contain digital
   signature information")

## Key evidence

- `audiodg.exe` has **zero** OGEQ modules loaded (verified via
  `Get-Process audiodg -Module`)
- `C:\ProgramData\OGEQ\apo-trace.log` is **never created**, despite the
  trace DLL being correctly registered
- The engine called `DllGetClassObject` exactly **once** (2026-10-02
  23:21:31) then never again
- The minimal test APO also fails to load → systemic, not code-specific

## Conclusion

Windows 11 24H2 on this machine will not load **any** unsigned third-party
APO, regardless of implementation correctness. `DisableProtectedAudioDG=1`
is insufficient. The INF installation path also requires a digital signature.

Likely root cause: unsigned code rejection at the OS policy level.

## Paths forward (if resumed)

1. **Test-signing mode**: `bcdedit /set testsigning on` + reboot, then retry
   the INF install. Tells us if signing is the only blocker.
2. **Self-signed certificate**: sign the DLL and INF catalog, install the
   cert as trusted. Works for local testing.
3. **Commercial code-signing certificate**: required for real distribution
   (~$200–300/year).
4. **Investigate 24H2 policy changes**: why `DisableProtectedAudioDG` no
   longer suffices, and whether an additional policy governs APO loading.

## Commits in this investigation

- `03f7cc0` — Attach: Win11 chain slots 13/14/15
- `83f627d` — Attach: fix premature RegCloseKey
- `21d7735` — APO: missing registry values + factory instrumentation
- `e2033cc` — APO: implement IAudioSystemEffects + PID/path logging
- `829fe8d` — Attach: detach clears Win11 chain slots
- `25742ca` — APO: new CLSID (blacklist test)
- `53dee70` — Minimal test APO
- `9df0dae` — Minimal APO build fixes
- `9bdd09f` — Attach: --clsid override
- `915d885` — INF installer
