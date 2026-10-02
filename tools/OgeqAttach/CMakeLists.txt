add_executable(ogeq_attach OgeqAttach.cpp)
set_target_properties(ogeq_attach PROPERTIES
    OUTPUT_NAME "ogeq_attach"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
target_link_libraries(ogeq_attach PRIVATE advapi32)
