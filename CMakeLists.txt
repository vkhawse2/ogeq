# OGEQ -- system-wide Windows audio equalizer
#
# Single source of truth for the app name. To rename the entire app:
#     python3 rename_app.py <NewName>
# That script updates this variable plus every source file, filename,
# and installer reference. Do not rename by hand.

cmake_minimum_required(VERSION 3.20)

set(APP_NAME "OGEQ")

project(${APP_NAME} LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "${APP_NAME} builds on Windows only (MSVC + Windows SDK).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Static CRT everywhere (/MT). The APO DLL runs inside audiodg.exe which has
# no VC++ runtime beside it, and mixing MD/MT across linked libs is a
# LNK2038 error. So: everything static.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Build id stamped into binaries (short commit SHA from CI, "dev" locally).
if(NOT DEFINED OGEQ_BUILD_ID)
    set(OGEQ_BUILD_ID "dev")
endif()

# Version: bump for every handed-over build.
set(OGEQ_VERSION "0.1.0")

add_compile_definitions(
    OGEQ_APP_NAME="${APP_NAME}"
    OGEQ_VERSION="${OGEQ_VERSION}"
    OGEQ_BUILD_ID="${OGEQ_BUILD_ID}"
)

include(CTest)
enable_testing()

# Phase 1: DSP engine + tests build everywhere.
# APO needs the Windows WDK (audioenginebaseapo.h) -- opt-in.
# UI only needs the Windows SDK (on CI runners) -- on by default.
option(OGEQ_BUILD_APO "Build the APO DLL (needs WDK)" OFF)
option(OGEQ_BUILD_UI "Build the Win32 UI" ON)

add_subdirectory(AudioEngine)
add_subdirectory(Tests)
add_subdirectory(Shared)
add_subdirectory(tools)
if(OGEQ_BUILD_APO)
    add_subdirectory(APO)
endif()
if(OGEQ_BUILD_UI)
    add_subdirectory(UI)
endif()
# Service/ and SpatialAudio/ are Phase 3/4; their CMakeLists are stubs.
