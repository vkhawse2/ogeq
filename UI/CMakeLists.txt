add_executable(ogeq_ui WIN32
    main.cpp
    OgeqUI.cpp
)

set_target_properties(ogeq_ui PROPERTIES
    OUTPUT_NAME "OGEQ"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

target_link_libraries(ogeq_ui PRIVATE comctl32)
