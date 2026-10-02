add_executable(ogeq_comtest ComTest.cpp)
set_target_properties(ogeq_comtest PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
