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

add_subdirectory(AudioEngine)
add_subdirectory(Tests)
add_subdirectory(APO)
add_subdirectory(UI)
# Service/ and SpatialAudio/ are Phase 3/4; their CMakeLists are stubs.
