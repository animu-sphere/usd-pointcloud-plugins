if(NOT DEFINED probe OR NOT DEFINED output)
    message(FATAL_ERROR "runtime LOD payload probe test arguments are incomplete")
endif()

file(REMOVE_RECURSE "${output}")
execute_process(
    COMMAND "${probe}" --output "${output}/PointCloud.usda"
    RESULT_VARIABLE probe_result
    OUTPUT_VARIABLE probe_output
    ERROR_VARIABLE probe_error)
if(NOT probe_result EQUAL 0)
    message(FATAL_ERROR
        "runtime LOD payload probe failed: ${probe_output}${probe_error}")
endif()

string(REPLACE "\r\n" "\n" normalized_probe_output "${probe_output}")
string(STRIP "${normalized_probe_output}" normalized_probe_output)
string(REPLACE "\n" ";" probe_lines "${normalized_probe_output}")
list(GET probe_lines -1 probe_row)
string(REPLACE "\t" ";" probe_fields "${probe_row}")
list(LENGTH probe_fields probe_field_count)
if(NOT probe_field_count EQUAL 10)
    message(FATAL_ERROR
        "probe output has unexpected field count: ${probe_row}")
endif()
list(GET probe_fields 1 authored_lods)
list(GET probe_fields 2 authored_payloads)
list(GET probe_fields 3 load_none_loaded_lods)
list(GET probe_fields 4 selective_loaded_lods)
list(GET probe_fields 5 selective_loaded_points)
list(GET probe_fields 6 load_all_loaded_lods)
list(GET probe_fields 7 load_all_points)
list(GET probe_fields 8 default_index)
list(GET probe_fields 9 thresholds)
if(NOT authored_lods STREQUAL "3" OR
   NOT authored_payloads STREQUAL "3" OR
   NOT load_none_loaded_lods STREQUAL "0" OR
   NOT selective_loaded_lods STREQUAL "1" OR
   NOT selective_loaded_points STREQUAL "2" OR
   NOT load_all_loaded_lods STREQUAL "3" OR
   NOT load_all_points STREQUAL "6" OR
   NOT default_index STREQUAL "2" OR
   NOT thresholds STREQUAL "0.25,0.1")
    message(FATAL_ERROR
        "probe output has unexpected composition results: ${probe_row}")
endif()
