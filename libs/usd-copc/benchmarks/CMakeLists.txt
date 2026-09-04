add_executable(usdCopc_runtime_streaming_probe
    runtime_streaming_probe.cpp)
target_link_libraries(usdCopc_runtime_streaming_probe
    PRIVATE usdcopc::core)

if(MSVC)
    target_compile_options(usdCopc_runtime_streaming_probe PRIVATE /utf-8)
endif()

if(USDGEO_BUILD_TESTS)
    add_dependencies(usdCopc_runtime_streaming_probe usdCopc_tests)
    add_test(NAME usdCopc_runtime_streaming_probe
             COMMAND ${CMAKE_COMMAND}
                     -Dprobe=$<TARGET_FILE:usdCopc_runtime_streaming_probe>
                     -Dfixture=${CMAKE_CURRENT_BINARY_DIR}/runtime-streaming.copc
                     -Dfixture_generator=$<TARGET_FILE:usdCopc_tests>
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/test_runtime_streaming_probe.cmake)
endif()