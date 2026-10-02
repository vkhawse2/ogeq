add_executable(ogeq_dsp_tests DspTests.cpp)
target_link_libraries(ogeq_dsp_tests PRIVATE ogeq_dsp)

add_test(NAME dsp_tests COMMAND ogeq_dsp_tests)

# The DSP lib is platform-independent; these tests run on any CI runner.
# The APO/UI build only on Windows (MSVC + Windows SDK).
