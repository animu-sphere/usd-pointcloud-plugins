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

if(USDGEO_BUILD_TESTS)
    add_test(NAME usdPointCloudAuthoring_stream_benchmark_las
             COMMAND ${CMAKE_COMMAND}
                     -Dbenchmark=$<TARGET_FILE:usdPointCloudAuthoring_stream_benchmark>
                     -Dfixture=${CMAKE_SOURCE_DIR}/plugins/pointcloud-las/tests/fixtures/conformance.las
                     -Dtest_root=${CMAKE_CURRENT_BINARY_DIR}/benchmark-conformance-las
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/test_stream_benchmark.cmake)
    add_test(NAME usdPointCloudAuthoring_stream_benchmark_laz
             COMMAND ${CMAKE_COMMAND}
                     -Dbenchmark=$<TARGET_FILE:usdPointCloudAuthoring_stream_benchmark>
                     -Dfixture=${CMAKE_SOURCE_DIR}/plugins/pointcloud-laz/tests/fixtures/conformance.laz
                     -Dtest_root=${CMAKE_CURRENT_BINARY_DIR}/benchmark-conformance-laz
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/test_stream_benchmark.cmake)
endif()