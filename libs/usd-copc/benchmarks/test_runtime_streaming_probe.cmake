if(NOT DEFINED probe OR NOT DEFINED fixture OR
   NOT DEFINED fixture_generator)
    message(FATAL_ERROR "runtime streaming probe test arguments are incomplete")
endif()

file(REMOVE_RECURSE "${fixture}")
execute_process(
    COMMAND "${fixture_generator}" --write-fixture "${fixture}"
    RESULT_VARIABLE fixture_result
    OUTPUT_VARIABLE fixture_output
    ERROR_VARIABLE fixture_error)
if(NOT fixture_result EQUAL 0)
    message(FATAL_ERROR
        "runtime streaming fixture generation failed: "
        "${fixture_output}${fixture_error}")
endif()

execute_process(
    COMMAND "${probe}" "${fixture}" --level 0
    RESULT_VARIABLE probe_result
    OUTPUT_VARIABLE probe_output
    ERROR_VARIABLE probe_error)
if(NOT probe_result EQUAL 0)
    message(FATAL_ERROR
        "runtime streaming probe failed: ${probe_output}${probe_error}")
endif()
if(NOT probe_output MATCHES "hierarchy_nodes")
    message(FATAL_ERROR "probe output is missing its TSV header: ${probe_output}")
endif()
if(NOT probe_output MATCHES "selected_point_range_bytes")
    message(FATAL_ERROR "probe output is missing range accounting: ${probe_output}")
endif()
if(NOT probe_output MATCHES "selected_tile_ids")
    message(FATAL_ERROR "probe output is missing tile identities: ${probe_output}")
endif()
if(NOT probe_output MATCHES "selected_tile_ranges")
    message(FATAL_ERROR "probe output is missing tile ranges: ${probe_output}")
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
list(GET probe_fields 1 reported_level)
list(GET probe_fields 2 reported_hierarchy_nodes)
list(GET probe_fields 3 reported_point_tiles)
list(GET probe_fields 4 reported_selected_tiles)
list(GET probe_fields 5 reported_decoded_points)
list(GET probe_fields 6 reported_range_bytes)
list(GET probe_fields 7 reported_source_bytes)
list(GET probe_fields 8 reported_tile_ids)
list(GET probe_fields 9 reported_tile_ranges)
if(NOT reported_level STREQUAL "0" OR
   NOT reported_hierarchy_nodes MATCHES "^[0-9]+$" OR
   NOT reported_point_tiles STREQUAL "1" OR
   NOT reported_selected_tiles STREQUAL "1" OR
   NOT reported_decoded_points STREQUAL "3" OR
   NOT reported_range_bytes MATCHES "^[1-9][0-9]*$" OR
   NOT reported_source_bytes MATCHES "^[1-9][0-9]*$" OR
   NOT reported_tile_ids STREQUAL "L0/0/0/0" OR
   NOT reported_tile_ranges MATCHES "^[1-9][0-9]*:[1-9][0-9]*$")
    message(FATAL_ERROR
        "probe output has unexpected level-0 results: ${probe_row}")
endif()