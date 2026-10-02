add_executable(ogeq_ui WIN32
    main.cpp
)

set_target_properties(ogeq_ui PROPERTIES
    OUTPUT_NAME "OGEQ"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
