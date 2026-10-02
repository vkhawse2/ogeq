# OGEQ

System-wide Windows 11 audio equalizer — native C++, APO-based DSP, minimal RAM footprint.

## Architecture

```
OGEQ/
├── AudioEngine/   # DSP library: biquad filters, parametric EQ, limiter (Phase 1)
├── APO/           # Windows Audio Processing Object (Phase 2)
├── UI/            # Native Win32 UI (Phase 3)
├── Service/       # Background service (Phase 3)
├── Shared/        # UI<->APO IPC contract
├── SpatialAudio/  # HRTF, virtual surround (Phase 4)
├── Installer/     # WiX MSI (Phase 5)
└── Tests/         # DSP tests
```

See [ROADMAP.md](ROADMAP.md) for the phased plan and the hard-won lessons
carried over from the MiniEQ lineage.

## Renaming the app

One command:

```bash
python3 rename_app.py <NewName>
```

Updates the CMake `APP_NAME`, every source file, filenames, and installer
references. Rebuild after renaming.

## Building (Windows, MSVC)

```bat
cmake -S . -B build -DOGEQ_BUILD_ID=dev
cmake --build build --config Release
ctest --test-dir build -C Release
```

## Status

Phase 1 in progress — DSP engine headers and biquad implementations are in;
the test harness is next.
