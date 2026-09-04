if(NOT DEFINED converter OR NOT DEFINED fixture OR
   NOT DEFINED fixture_generator OR NOT DEFINED test_root)
    message(FATAL_ERROR "COPC converter test arguments are incomplete")
endif()

file(REMOVE_RECURSE "${test_root}")
execute_process(
    COMMAND "${fixture_generator}" --write-fixture "${fixture}"
    RESULT_VARIABLE fixture_result
    ERROR_VARIABLE fixture_error)
if(NOT fixture_result EQUAL 0)
    message(FATAL_ERROR
        "COPC fixture generation failed: ${fixture_error}")
endif()

set(cache_root "${test_root}/cache")
foreach(pass IN ITEMS first second)
    set(output_directory "${test_root}/${pass}")
    execute_process(
        COMMAND "${converter}" "${fixture}"
                "${output_directory}/PointCloud.usda"
                --tile-size 1 --memory-limit 1024
                --cache-root "${cache_root}"
        RESULT_VARIABLE convert_result
        OUTPUT_VARIABLE convert_output
        ERROR_VARIABLE convert_error)
    if(NOT convert_result EQUAL 0 OR NOT convert_error STREQUAL "" OR
       NOT EXISTS "${output_directory}/PointCloud.usda" OR
       NOT EXISTS "${output_directory}/payloads/tiles.manifest")
        message(FATAL_ERROR
            "COPC ${pass} conversion failed: ${convert_output}${convert_error}")
    endif()
    if(pass STREQUAL "first")
        string(FIND "${convert_output}"
            "Cache lookups: 1, hits: 0, misses: 1" cache_stats_position)
    else()
        string(FIND "${convert_output}"
            "Cache lookups: 1, hits: 1, misses: 0" cache_stats_position)
    endif()
    if(cache_stats_position EQUAL -1)
        message(FATAL_ERROR
            "COPC ${pass} conversion has unexpected cache statistics: ${convert_output}")
    endif()
endforeach()

set(copc_laz_fixture "${test_root}/converter-conformance.copc.laz")
file(COPY_FILE "${fixture}" "${copc_laz_fixture}")
execute_process(
    COMMAND "${converter}" "${copc_laz_fixture}"
            "${test_root}/copc-laz/PointCloud.usda"
            --tile-size 1 --memory-limit 1024
    RESULT_VARIABLE copc_laz_result
    ERROR_VARIABLE copc_laz_error)
if(NOT copc_laz_result EQUAL 0 OR NOT copc_laz_error STREQUAL "" OR
   NOT EXISTS "${test_root}/copc-laz/PointCloud.usda")
    message(FATAL_ERROR
        "COPC LAZ extension conversion failed: ${copc_laz_error}")
endif()