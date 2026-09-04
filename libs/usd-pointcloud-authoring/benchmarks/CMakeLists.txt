add_executable(usdPointCloudAuthoring_stream_benchmark
    stream_benchmark.cpp)
target_link_libraries(usdPointCloudAuthoring_stream_benchmark
    PRIVATE
        usdpointcloud::authoring
        usdcopc::core
        usdlas::core
        usdlaz::core
        usdply::core)

if(WIN32)
    target_link_libraries(usdPointCloudAuthoring_stream_benchmark PRIVATE Psapi)
endif()

add_executable(usdPointCloudAuthoring_runtime_lod_payload_probe
    runtime_lod_payload_probe.cpp)
target_link_libraries(usdPointCloudAuthoring_runtime_lod_payload_probe
    PRIVATE usdpointcloud::authoring)

if(USDGEO_BUILD_TESTS)
    add_dependencies(usdPointCloudAuthoring_stream_benchmark usdCopc_tests)
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
        add_test(NAME usdPointCloudAuthoring_stream_benchmark_ply
               COMMAND ${CMAKE_COMMAND}
                     -Dbenchmark=$<TARGET_FILE:usdPointCloudAuthoring_stream_benchmark>
                     -Dfixture=${CMAKE_SOURCE_DIR}/plugins/pointcloud-ply/tests/fixtures/conformance.ply
                     -Depsg=26910
                     -Dtest_root=${CMAKE_CURRENT_BINARY_DIR}/benchmark-conformance-ply
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/test_stream_benchmark.cmake)
    add_test(NAME usdPointCloudAuthoring_stream_benchmark_copc
             COMMAND ${CMAKE_COMMAND}
                     -Dbenchmark=$<TARGET_FILE:usdPointCloudAuthoring_stream_benchmark>
                     -Dfixture=${CMAKE_CURRENT_BINARY_DIR}/benchmark-conformance.copc
                     -Dfixture_generator=$<TARGET_FILE:usdCopc_tests>
                     -Dtest_root=${CMAKE_CURRENT_BINARY_DIR}/benchmark-conformance-copc
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/test_stream_benchmark.cmake)
        add_test(NAME usdPointCloudAuthoring_stream_benchmark_cross_format
             COMMAND ${CMAKE_COMMAND}
                 -Dbenchmark=$<TARGET_FILE:usdPointCloudAuthoring_stream_benchmark>
                 -Dlas_fixture=${CMAKE_SOURCE_DIR}/plugins/pointcloud-las/tests/fixtures/conformance.las
                 -Dlaz_fixture=${CMAKE_SOURCE_DIR}/plugins/pointcloud-laz/tests/fixtures/conformance.laz
                 -Dply_fixture=${CMAKE_SOURCE_DIR}/plugins/pointcloud-ply/tests/fixtures/conformance.ply
                 -Dcopc_fixture=${CMAKE_CURRENT_BINARY_DIR}/benchmark-conformance.copc
                 -Dfixture_generator=$<TARGET_FILE:usdCopc_tests>
                 -Dtest_root=${CMAKE_CURRENT_BINARY_DIR}/benchmark-cross-format
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/compare_stream_benchmark.cmake)
    add_test(NAME usdPointCloudAuthoring_runtime_lod_payload_probe
             COMMAND ${CMAKE_COMMAND}
                     -Dprobe=$<TARGET_FILE:usdPointCloudAuthoring_runtime_lod_payload_probe>
                     -Doutput=${CMAKE_CURRENT_BINARY_DIR}/runtime-lod-payload
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/test_runtime_lod_payload_probe.cmake)
    usdgeo_configure_openusd_test_runtime(
        usdPointCloudAuthoring_runtime_lod_payload_probe)
endif()