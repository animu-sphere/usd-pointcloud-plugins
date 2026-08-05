if(NOT DEFINED converter OR NOT DEFINED fixture OR NOT DEFINED test_root)
    message(FATAL_ERROR "converter test arguments are incomplete")
endif()

file(REMOVE_RECURSE "${test_root}")
file(MAKE_DIRECTORY "${test_root}")
set(output_root "${test_root}/PointCloud.usda")
set(output_payloads "${test_root}/PointCloud_payloads")

execute_process(
    COMMAND "${converter}" "${fixture}" "${output_root}"
            --tile-size 1 --memory-limit 1024
            --attributes xyz,intensity
    RESULT_VARIABLE convert_result
    OUTPUT_VARIABLE convert_output
    ERROR_VARIABLE convert_error)
if(NOT convert_result EQUAL 0)
    message(FATAL_ERROR
        "converter failed with ${convert_result}: ${convert_output}${convert_error}")
endif()
if(NOT convert_error STREQUAL "")
    message(FATAL_ERROR "converter emitted diagnostics: ${convert_error}")
endif()
if(NOT EXISTS "${output_root}" OR NOT EXISTS "${output_payloads}")
    message(FATAL_ERROR "converter did not publish the expected output bundle")
endif()

execute_process(
    COMMAND usdcat --flatten "${output_root}" -o "${test_root}/flattened.usda"
    RESULT_VARIABLE usdcat_result
    OUTPUT_VARIABLE usdcat_output
    ERROR_VARIABLE usdcat_error)
if(NOT usdcat_result EQUAL 0)
    message(FATAL_ERROR
        "generated root could not be reopened: ${usdcat_output}${usdcat_error}")
endif()

file(REMOVE_RECURSE "${test_root}")