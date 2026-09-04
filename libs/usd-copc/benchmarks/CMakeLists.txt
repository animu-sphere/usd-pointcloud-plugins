add_executable(usdCopc_runtime_streaming_probe
    runtime_streaming_probe.cpp)
target_link_libraries(usdCopc_runtime_streaming_probe
    PRIVATE usdcopc::core)

if(MSVC)
    target_compile_options(usdCopc_runtime_streaming_probe PRIVATE /utf-8)
endif()