if(NOT DEFINED benchmark OR NOT DEFINED fixture OR NOT DEFINED test_root)
    message(FATAL_ERROR "benchmark test arguments are incomplete")
endif()

file(REMOVE_RECURSE "${test_root}")

set(epsg_arguments)
if(DEFINED epsg AND NOT epsg STREQUAL "")
    list(APPEND epsg_arguments "--epsg" "${epsg}")
endif()

execute_process(
    COMMAND "${benchmark}" "--input" "${fixture}"
            "--chunk-points" "2" "--tile-size" "1" "--memory-limit" "1024"
            ${epsg_arguments}
    RESULT_VARIABLE benchmark_result
    OUTPUT_VARIABLE benchmark_output
    ERROR_VARIABLE benchmark_error)
if(NOT benchmark_result EQUAL 0)
    message(FATAL_ERROR
        "benchmark failed with ${benchmark_result}: "
        "${benchmark_output}${benchmark_error}")
endif()
if(NOT benchmark_output MATCHES "tile_count=[1-9][0-9]*")
    message(FATAL_ERROR "benchmark output is missing a positive tile count: ${benchmark_output}")
endif()
if(NOT benchmark_output MATCHES "payload_bytes=[1-9][0-9]*")
    message(FATAL_ERROR "benchmark output is missing positive payload bytes: ${benchmark_output}")
endif()
if(NOT benchmark_output MATCHES "success=true")
    message(FATAL_ERROR "benchmark output did not report success: ${benchmark_output}")
endif()