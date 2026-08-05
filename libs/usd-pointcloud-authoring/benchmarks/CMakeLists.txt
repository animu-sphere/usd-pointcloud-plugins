add_executable(usdPointCloudAuthoring_stream_benchmark
    stream_benchmark.cpp)
target_link_libraries(usdPointCloudAuthoring_stream_benchmark
    PRIVATE
        usdpointcloud::authoring
        usdlas::core
        usdlaz::core)

if(WIN32)
    target_link_libraries(usdPointCloudAuthoring_stream_benchmark PRIVATE Psapi)
endif()