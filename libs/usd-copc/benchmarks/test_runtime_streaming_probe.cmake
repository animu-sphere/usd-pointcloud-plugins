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
if(NOT probe_output MATCHES "\\t0\\t[0-9]+\\t1\\t1\\t3\\t[1-9][0-9]*\\t[1-9][0-9]*")
    message(FATAL_ERROR
        "probe output has unexpected level-0 results: ${probe_output}")
endif()