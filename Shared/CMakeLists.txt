# Shared: IPC contract + status channel (header-only for contract, compiled for channel).
add_library(ogeq_shared STATIC
    OgeqStatusChannel.cpp
)

target_include_directories(ogeq_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
