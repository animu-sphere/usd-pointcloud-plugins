if(NOT DEFINED converter OR NOT DEFINED fixture OR
   NOT DEFINED fixture_generator OR NOT DEFINED test_root OR
   NOT DEFINED resolver_plugin_path)
    message(FATAL_ERROR "resolver conversion test arguments are incomplete")
endif()

file(REMOVE_RECURSE "${test_root}")
execute_process(
    COMMAND "${fixture_generator}" --write-fixture "${fixture}"
    RESULT_VARIABLE fixture_result
    ERROR_VARIABLE fixture_error)
if(NOT fixture_result EQUAL 0)
    message(FATAL_ERROR
        "Resolver COPC fixture generation failed: ${fixture_error}")
endif()

set(cache_root "${test_root}/cache")
foreach(pass IN ITEMS first second)
    set(output_directory "${test_root}/${pass}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "PXR_AR_DEFAULT_RESOLVER=HttpResolver"
                "PXR_PLUGINPATH_NAME=${resolver_plugin_path}"
                "USDGEOCOPC_TEST_ASSET=${fixture}"
                "${converter}" "http://memory.copc"
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
            "Resolver COPC ${pass} conversion failed: ${convert_output}${convert_error}")
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
            "Resolver COPC ${pass} has unexpected cache statistics: ${convert_output}")
    endif()

    file(READ "${output_directory}/PointCloud.usda.manifest" manifest)
    string(FIND "${manifest}" "memory.copc" identifier_position)
    string(FIND "${manifest}" "test-fnv1a64" token_position)
    if(NOT identifier_position EQUAL -1 OR NOT token_position EQUAL -1)
        message(FATAL_ERROR
            "Resolver identifier or validation token leaked into manifest")
    endif()
endforeach()

set(unstable_output_directory "${test_root}/unstable")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
            "PXR_AR_DEFAULT_RESOLVER=HttpResolver"
            "PXR_PLUGINPATH_NAME=${resolver_plugin_path}"
            "USDGEOCOPC_TEST_ASSET=${fixture}"
            "USDGEOCOPC_TEST_IDENTITY=unstable"
            "${converter}" "http://memory.copc"
            "${unstable_output_directory}/PointCloud.usda"
            --tile-size 1 --memory-limit 1024
            --cache-root "${cache_root}"
    RESULT_VARIABLE unstable_result
    OUTPUT_VARIABLE unstable_output
    ERROR_VARIABLE unstable_error)
if(NOT unstable_result EQUAL 0 OR NOT unstable_error STREQUAL "" OR
   NOT EXISTS "${unstable_output_directory}/PointCloud.usda" OR
   NOT EXISTS "${unstable_output_directory}/payloads/tiles.manifest")
    message(FATAL_ERROR
        "Unstable resolver COPC conversion failed: ${unstable_output}${unstable_error}")
endif()
string(FIND "${unstable_output}" "Cache lookups:" unstable_cache_position)
if(NOT unstable_cache_position EQUAL -1)
    message(FATAL_ERROR
        "Unstable resolver identity unexpectedly enabled generated cache")
endif()
