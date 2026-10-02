# Test player: hear the DSP on a WAV file before it touches Windows.
# Builds everywhere (no Windows SDK needed).
add_executable(ogeq_player
    main.cpp
    WavFile.cpp
)
target_link_libraries(ogeq_player PRIVATE ogeq_dsp)
target_include_directories(ogeq_player PRIVATE
    ${CMAKE_SOURCE_DIR}/AudioEngine/include
)
set_target_properties(ogeq_player PROPERTIES OUTPUT_NAME "ogeq_player")
