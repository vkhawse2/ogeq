add_library(ogeq_dsp STATIC
    src/BiquadFilter.cpp
    src/Equalizer.cpp
    src/DspProcessor.cpp
)

target_include_directories(ogeq_dsp PUBLIC include)

# The DSP library is deliberately dependency-free (no Windows headers),
# so it can be unit-tested on any platform.
