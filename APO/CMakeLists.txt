# APO DLL: static CRT (MiniEQ lesson -- audiodg has no VC++ runtime beside it).
# Requires the Windows WDK for audioenginebaseapo.h. Only built when
# OGEQ_BUILD_APO=ON (not on stock GitHub runners).
add_library(ogeq_apo SHARED
    OgeqApo.cpp
    dllmain.cpp
)

set_target_properties(ogeq_apo PROPERTIES
    OUTPUT_NAME "OGEQ_APO"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

target_link_libraries(ogeq_apo PRIVATE ogeq_dsp)

target_include_directories(ogeq_apo PRIVATE
    ${CMAKE_SOURCE_DIR}/AudioEngine/include
    ${CMAKE_SOURCE_DIR}/Shared
)

# APO registration metadata (used by DllRegisterServer + the UI attach flow):
#   CLSID: {b8122668-b395-481b-a516-03d4014e4421}
#   u32MaxInstances = 1 (24H2 graph-builder stability lesson)
#   APOInterface0 = IID_IAudioProcessingObject
