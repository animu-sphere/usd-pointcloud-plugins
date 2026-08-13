if(NOT DEFINED benchmark OR NOT DEFINED test_root)
    message(FATAL_ERROR "cross-format benchmark arguments are incomplete")
endif()

foreach(format IN ITEMS las laz ply copc)
    if(NOT DEFINED ${format}_fixture)
        message(FATAL_ERROR "missing ${format} benchmark fixture")
    endif()
endforeach()

file(REMOVE_RECURSE "${test_root}")
file(MAKE_DIRECTORY "${test_root}")

if(DEFINED fixture_generator AND NOT fixture_generator STREQUAL "")
    execute_process(
        COMMAND "${fixture_generator}" "--write-fixture" "${copc_fixture}"
        RESULT_VARIABLE fixture_result
        ERROR_VARIABLE fixture_error)
    if(NOT fixture_result EQUAL 0)
        message(FATAL_ERROR
            "fixture generation failed with ${fixture_result}: ${fixture_error}")
    endif()
endif()

set(report "${test_root}/cross-format.tsv")
file(WRITE "${report}"
    "format\tstrategy\tpoints\ttile_count\ttile_manifest_count\t"
    "tile_point_min\ttile_point_max\ttile_point_average\t"
    "tile_payload_min_bytes\ttile_payload_max_bytes\t"
    "tile_payload_average_bytes\ttree_depth\tpeak_rss_bytes\tpayload_bytes\t"
    "source_read_bytes\tspool_bytes_written\tspool_bytes_read\t"
    "io_amplification\telapsed_seconds\n")

foreach(strategy IN ITEMS fixed-grid adaptive)
foreach(format IN ITEMS las laz ply copc)
    set(epsg_arguments)
    if(format STREQUAL "ply")
        list(APPEND epsg_arguments "--epsg" "26910")
    endif()
    set(strategy_arguments "--tiling" "${strategy}")
    if(strategy STREQUAL "adaptive")
        list(APPEND strategy_arguments
             "--max-points-per-tile" "2"
             "--min-points-per-tile" "1"
             "--max-depth" "8")
    endif()

    execute_process(
        COMMAND "${benchmark}" "--input" "${${format}_fixture}"
                "--format" "${format}"
                "--chunk-points" "2" "--tile-size" "1"
                "--memory-limit" "1024" ${strategy_arguments}
                ${epsg_arguments}
        RESULT_VARIABLE benchmark_result
        OUTPUT_VARIABLE benchmark_output
        ERROR_VARIABLE benchmark_error)
    if(NOT benchmark_result EQUAL 0)
        message(FATAL_ERROR
            "${format} benchmark failed with ${benchmark_result}: "
            "${benchmark_output}${benchmark_error}")
    endif()

    string(REGEX MATCH "format=([^ ]+)" _match "${benchmark_output}")
    set(reported_format "${CMAKE_MATCH_1}")
    string(REGEX MATCH "tiling=([^ ]+)" _match "${benchmark_output}")
    set(reported_strategy "${CMAKE_MATCH_1}")
    string(REGEX MATCH "points=([0-9]+)" _match "${benchmark_output}")
    set(reported_points "${CMAKE_MATCH_1}")
    string(REGEX MATCH "tile_count=([0-9]+)" _match "${benchmark_output}")
    set(reported_tile_count "${CMAKE_MATCH_1}")
    string(REGEX MATCH "tile_manifest_count=([0-9]+)" _match "${benchmark_output}")
    set(reported_tile_manifest_count "${CMAKE_MATCH_1}")
    string(REGEX MATCH "tile_point_min=([0-9]+)" _match "${benchmark_output}")
    set(reported_tile_point_min "${CMAKE_MATCH_1}")
    string(REGEX MATCH "tile_point_max=([0-9]+)" _match "${benchmark_output}")
    set(reported_tile_point_max "${CMAKE_MATCH_1}")
    string(REGEX MATCH "tile_point_average=([0-9]+(\.[0-9]+)?)" _match "${benchmark_output}")
    set(reported_tile_point_average "${CMAKE_MATCH_1}")
    string(REGEX MATCH "tile_payload_min_bytes=([0-9]+)" _match "${benchmark_output}")
    set(reported_tile_payload_min "${CMAKE_MATCH_1}")
    string(REGEX MATCH "tile_payload_max_bytes=([0-9]+)" _match "${benchmark_output}")
    set(reported_tile_payload_max "${CMAKE_MATCH_1}")
    string(REGEX MATCH "tile_payload_average_bytes=([0-9]+(\.[0-9]+)?)" _match "${benchmark_output}")
    set(reported_tile_payload_average "${CMAKE_MATCH_1}")
    string(REGEX MATCH "tree_depth=([0-9]+)" _match "${benchmark_output}")
    set(reported_tree_depth "${CMAKE_MATCH_1}")
    string(REGEX MATCH "peak_rss_bytes=([0-9]+)" _match "${benchmark_output}")
    set(reported_peak_rss "${CMAKE_MATCH_1}")
    string(REGEX MATCH "payload_bytes=([0-9]+)" _match "${benchmark_output}")
    set(reported_payload_bytes "${CMAKE_MATCH_1}")
    string(REGEX MATCH "source_read_bytes=([0-9]+)" _match "${benchmark_output}")
    set(reported_source_read_bytes "${CMAKE_MATCH_1}")
    string(REGEX MATCH "spool_bytes_written=([0-9]+)" _match "${benchmark_output}")
    set(reported_spool_bytes_written "${CMAKE_MATCH_1}")
    string(REGEX MATCH "spool_bytes_read=([0-9]+)" _match "${benchmark_output}")
    set(reported_spool_bytes_read "${CMAKE_MATCH_1}")
    string(REGEX MATCH "io_amplification=([0-9]+\\.[0-9]+)" _match "${benchmark_output}")
    set(reported_io_amplification "${CMAKE_MATCH_1}")
    string(REGEX MATCH "elapsed_seconds=([0-9.]+)" _match "${benchmark_output}")
    set(reported_elapsed "${CMAKE_MATCH_1}")
    string(REGEX MATCH "success=true" reported_success "${benchmark_output}")
    if(NOT reported_format OR NOT reported_strategy OR NOT reported_points OR
             NOT reported_tile_count OR NOT reported_tile_manifest_count OR
             NOT reported_tile_point_min OR NOT reported_tile_point_max OR
             NOT reported_tile_point_average OR NOT reported_tile_payload_min OR
             NOT reported_tile_payload_max OR
             NOT reported_tile_payload_average OR NOT reported_tree_depth OR
             NOT reported_peak_rss OR NOT reported_payload_bytes OR
             NOT reported_source_read_bytes OR NOT reported_spool_bytes_written OR
             NOT reported_spool_bytes_read OR NOT reported_io_amplification OR
             NOT reported_elapsed OR NOT reported_success)
        message(FATAL_ERROR
            "${format} benchmark output is not comparable: ${benchmark_output}")
    endif()
    if(NOT reported_format STREQUAL format)
        message(FATAL_ERROR
            "${format} benchmark reported format ${reported_format}")
    endif()
    if(NOT reported_strategy STREQUAL strategy)
        message(FATAL_ERROR
            "${format} benchmark reported strategy ${reported_strategy}")
    endif()

    file(APPEND "${report}"
         "${reported_format}\t${reported_strategy}\t${reported_points}\t"
         "${reported_tile_count}\t${reported_tile_manifest_count}\t"
         "${reported_tile_point_min}\t${reported_tile_point_max}\t"
         "${reported_tile_point_average}\t${reported_tile_payload_min}\t"
         "${reported_tile_payload_max}\t${reported_tile_payload_average}\t"
         "${reported_tree_depth}\t"
         "${reported_peak_rss}\t${reported_payload_bytes}\t"
         "${reported_source_read_bytes}\t${reported_spool_bytes_written}\t"
         "${reported_spool_bytes_read}\t${reported_io_amplification}\t"
         "${reported_elapsed}\n")
endforeach()
endforeach()

message(STATUS "cross-format benchmark report: ${report}")