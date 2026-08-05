if(NOT DEFINED converter OR NOT DEFINED fixture OR NOT DEFINED test_root)
    message(FATAL_ERROR "converter test arguments are incomplete")
endif()

file(REMOVE_RECURSE "${test_root}")
file(MAKE_DIRECTORY "${test_root}")
set(output_root "${test_root}/PointCloud.usda")
set(output_payloads "${test_root}/PointCloud_payloads")
set(stale_payloads "${test_root}/StalePayloads")
set(output_manifest "${output_root}.manifest")
set(output_transaction "${output_root}.transaction")

file(MAKE_DIRECTORY "${stale_payloads}" "${output_transaction}")
file(WRITE "${output_manifest}" "stale manifest")
file(WRITE "${test_root}/PointCloud.tmp.usda" "stale root")
file(WRITE "${output_manifest}.tmp" "stale temporary manifest")
file(WRITE "${stale_payloads}/stale.usdc" "stale payload")
file(WRITE "${output_transaction}/state"
    "payloadDirectory=${stale_payloads}\n")

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
if(NOT EXISTS "${output_root}" OR NOT EXISTS "${output_payloads}" OR
    NOT EXISTS "${output_manifest}" OR EXISTS "${output_transaction}" OR
    EXISTS "${stale_payloads}")
    message(FATAL_ERROR "converter did not publish the expected output bundle")
endif()

file(READ "${output_manifest}" manifest_contents)
foreach(expected_line
        "format=usd-pointcloud-manifest-v1"
        "input.file=conformance.las"
        "argument.tile=true"
        "argument.tileSize=1.000000"
        "argument.tileMemoryLimit=1024"
        "argument.attributes=intensity,xyz"
        "output.payloadDirectory=PointCloud_payloads"
        "payload.file=PointCloud_payloads/")
    string(FIND "${manifest_contents}" "${expected_line}" expected_position)
    if(expected_position EQUAL -1)
        message(FATAL_ERROR "manifest is missing: ${expected_line}")
    endif()
endforeach()

file(SHA256 "${output_manifest}" first_manifest_hash)

set(repeat_root "${test_root}-repeat")
file(MAKE_DIRECTORY "${repeat_root}")
execute_process(
    COMMAND "${converter}" "${fixture}" "${repeat_root}/PointCloud.usda"
            --tile-size 1 --memory-limit 1024
            --attributes xyz,intensity
    RESULT_VARIABLE repeat_result
    OUTPUT_VARIABLE repeat_output
    ERROR_VARIABLE repeat_error)
if(NOT repeat_result EQUAL 0)
    message(FATAL_ERROR
        "repeat converter failed with ${repeat_result}: ${repeat_output}${repeat_error}")
endif()
file(SHA256 "${repeat_root}/PointCloud.usda.manifest" repeat_manifest_hash)
if(NOT first_manifest_hash STREQUAL repeat_manifest_hash)
    message(FATAL_ERROR "converter manifest is not deterministic")
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
file(REMOVE_RECURSE "${repeat_root}")