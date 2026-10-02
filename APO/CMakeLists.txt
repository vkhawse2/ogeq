# APO DLL: static CRT (MiniEQ lesson -- audiodg has no VC++ runtime beside it).
add_library(ogeq_apo SHARED
    OgeqApo.cpp
    dllmain.cpp
)

set_target_properties(ogeq_apo PROPERTIES
    OUTPUT_NAME "OGEQ_APO"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

target_link_libraries(ogeq_apo PRIVATE ogeq_dsp)

# Fresh CLSID per build lineage: generate once with
#   powershell -c "[guid]::NewGuid()"
# and bake it into OgeqApo.h. Never reuse MiniEQ's CLSID.
