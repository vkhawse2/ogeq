add_executable(ogeq_heartbeat OgeqHeartbeat.cpp)
set_target_properties(ogeq_heartbeat PROPERTIES
    OUTPUT_NAME "ogeq_heartbeat"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
target_include_directories(ogeq_heartbeat PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(ogeq_heartbeat PRIVATE ogeq_shared)
