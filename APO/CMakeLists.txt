# APO DLL: static CRT (MiniEQ lesson -- audiodg has no VC++ runtime beside it).
# Requires the Windows WDK for audioenginebaseapo.h. Only built when
# OGEQ_BUILD_APO=ON (not on stock GitHub runners).
add_library(ogeq_apo SHARED
    OgeqApo.cpp
    dllmain.cpp
    OGEQ_APO.def
)

set_target_properties(ogeq_apo PROPERTIES
    OUTPUT_NAME "OGEQ_APO"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

target_link_libraries(ogeq_apo PRIVATE ogeq_dsp ogeq_shared)

target_include_directories(ogeq_apo PRIVATE
    ${CMAKE_SOURCE_DIR}/AudioEngine/include
    ${CMAKE_SOURCE_DIR}/Shared
)

# WDK headers (audioenginebaseapo.h). On CI, provided via NuGet;
# locally, set WDK_INCLUDE_DIR to %ProgramFiles(x86)%\Windows Kits\10\Include\<ver>\shared
# or install the WDK.
if(DEFINED WDK_INCLUDE_DIR)
    target_include_directories(ogeq_apo PRIVATE ${WDK_INCLUDE_DIR})
endif()

# APO registration metadata (used by DllRegisterServer + the UI attach flow):
#   CLSID: {b8122668-b395-481b-a516-03d4014e4421}
#   u32MaxInstances = 1 (24H2 graph-builder stability lesson)
#   APOInterface0 = IID_IAudioProcessingObject

# Trace build: same DLL with file logging enabled (OGEQ_APO_TRACE).
# For debugging "engine won't load us" -- check C:\ProgramData\OGEQ\apo-trace.log
add_library(ogeq_apo_trace SHARED
    OgeqApo.cpp
    dllmain.cpp
    OGEQ_APO.def
)
set_target_properties(ogeq_apo_trace PROPERTIES
    OUTPUT_NAME "OGEQ_APO_trace"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
target_compile_definitions(ogeq_apo_trace PRIVATE OGEQ_APO_TRACE=1)
target_link_libraries(ogeq_apo_trace PRIVATE ogeq_dsp ogeq_shared)
target_include_directories(ogeq_apo_trace PRIVATE
    ${CMAKE_SOURCE_DIR}/AudioEngine/include
    ${CMAKE_SOURCE_DIR}/Shared
)
if(DEFINED WDK_INCLUDE_DIR)
    target_include_directories(ogeq_apo_trace PRIVATE ${WDK_INCLUDE_DIR})
endif()

# Minimal test APO: bare-bones pass-through for engine loading diagnosis.
# If this loads but OGEQ doesn't, the problem is in OGEQ's implementation.
# If this doesn't load either, the problem is systemic.
# Logs to C:\ProgramData\OGEQ\minimal-trace.log
# CLSID: {d4e5f6a7-b8c9-4d0e-af12-3456789abcde}
add_library(ogeq_minimal_apo SHARED
    Minimal/MinimalApo.cpp
    Minimal/MinimalApo.def
)

set_target_properties(ogeq_minimal_apo PROPERTIES
    OUTPUT_NAME "OGEQ_MinimalApo"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

if(DEFINED WDK_INCLUDE_DIR)
    target_include_directories(ogeq_minimal_apo PRIVATE ${WDK_INCLUDE_DIR})
endif()
