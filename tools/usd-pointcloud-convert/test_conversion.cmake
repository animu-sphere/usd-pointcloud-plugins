if(NOT DEFINED converter OR NOT DEFINED fixture OR NOT DEFINED test_root)
    message(FATAL_ERROR "converter test arguments are incomplete")
endif()

file(REMOVE_RECURSE "${test_root}")
file(MAKE_DIRECTORY "${test_root}")
set(output_root "${test_root}/PointCloud.usda")
set(output_payloads "${test_root}/PointCloud_payloads")
set(output_tile_manifest "${output_payloads}/tiles.manifest")
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
    NOT EXISTS "${output_manifest}" OR
    NOT EXISTS "${output_tile_manifest}" OR
    EXISTS "${output_transaction}" OR
    EXISTS "${stale_payloads}")
    message(FATAL_ERROR "converter did not publish the expected output bundle")
endif()

set(adaptive_root "${test_root}-adaptive")
file(REMOVE_RECURSE "${adaptive_root}")
execute_process(
    COMMAND "${converter}" "${fixture}" "${adaptive_root}/PointCloud.usda"
            --max-points-per-tile 1 --min-points-per-tile 1 --max-depth 8
            --memory-limit 1024 --attributes xyz,intensity
    RESULT_VARIABLE adaptive_result
    OUTPUT_VARIABLE adaptive_output
    ERROR_VARIABLE adaptive_error)
if(NOT adaptive_result EQUAL 0 OR NOT adaptive_error STREQUAL "" OR
   NOT EXISTS "${adaptive_root}/PointCloud.usda" OR
   NOT EXISTS "${adaptive_root}/PointCloud.usda.manifest" OR
    NOT EXISTS "${adaptive_root}/PointCloud_payloads" OR
    NOT EXISTS "${adaptive_root}/PointCloud_payloads/tiles.manifest")
    message(FATAL_ERROR
        "adaptive converter failed: ${adaptive_output}${adaptive_error}")
endif()
file(READ "${adaptive_root}/PointCloud.usda.manifest" adaptive_manifest)
foreach(expected_line
        "argument.maxPointsPerTile=1"
        "argument.minPointsPerTile=1"
        "argument.maxDepth=8")
    string(FIND "${adaptive_manifest}" "${expected_line}" expected_position)
    if(expected_position EQUAL -1)
        message(FATAL_ERROR "adaptive manifest is missing: ${expected_line}")
    endif()
endforeach()

set(adaptive_cache_root "${test_root}/adaptive-cache")
set(adaptive_cache_first_root "${test_root}-adaptive-cache-first")
set(adaptive_cache_second_root "${test_root}-adaptive-cache-second")
file(REMOVE_RECURSE "${adaptive_cache_root}" "${adaptive_cache_first_root}"
     "${adaptive_cache_second_root}")
file(MAKE_DIRECTORY "${adaptive_cache_first_root}"
     "${adaptive_cache_second_root}")
execute_process(
    COMMAND "${converter}" "${fixture}"
            "${adaptive_cache_first_root}/PointCloud.usda"
            --max-points-per-tile 1 --min-points-per-tile 1 --max-depth 8
            --memory-limit 1024 --attributes xyz,intensity
            --cache-root "${adaptive_cache_root}"
    RESULT_VARIABLE adaptive_cache_first_result
    OUTPUT_VARIABLE adaptive_cache_first_output
    ERROR_VARIABLE adaptive_cache_first_error)
string(FIND "${adaptive_cache_first_output}"
    "Cache lookups: 1, hits: 0, misses: 1"
    adaptive_cache_first_stats_position)
if(NOT adaptive_cache_first_result EQUAL 0 OR
   NOT adaptive_cache_first_error STREQUAL "" OR
   adaptive_cache_first_stats_position EQUAL -1)
    message(FATAL_ERROR
        "adaptive cached first conversion failed: "
        "${adaptive_cache_first_output}${adaptive_cache_first_error}")
endif()

execute_process(
    COMMAND "${converter}" "${fixture}"
            "${adaptive_cache_second_root}/PointCloud.usda"
            --max-points-per-tile 1 --min-points-per-tile 1 --max-depth 8
            --memory-limit 1024 --attributes intensity,xyz
            --cache-root "${adaptive_cache_root}"
    RESULT_VARIABLE adaptive_cache_second_result
    OUTPUT_VARIABLE adaptive_cache_second_output
    ERROR_VARIABLE adaptive_cache_second_error)
string(FIND "${adaptive_cache_second_output}" "Cache hit "
    adaptive_cache_hit_position)
string(FIND "${adaptive_cache_second_output}"
    "Cache lookups: 1, hits: 1, misses: 0"
    adaptive_cache_second_stats_position)
if(NOT adaptive_cache_second_result EQUAL 0 OR
   NOT adaptive_cache_second_error STREQUAL "" OR
   adaptive_cache_hit_position EQUAL -1 OR
   adaptive_cache_second_stats_position EQUAL -1 OR
   NOT EXISTS "${adaptive_cache_second_root}/PointCloud.usda" OR
   NOT EXISTS "${adaptive_cache_second_root}/payloads/tiles.manifest")
    message(FATAL_ERROR
        "adaptive converter did not reuse the cache entry: "
        "${adaptive_cache_second_output}${adaptive_cache_second_error}")
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

file(READ "${output_tile_manifest}" tile_manifest_contents)
foreach(expected_line
        "format=usd-pointcloud-tile-manifest-v1"
        "tile.count="
        "tile.0.id="
        "tile.0.payload=")
    string(FIND "${tile_manifest_contents}" "${expected_line}" expected_position)
    if(expected_position EQUAL -1)
        message(FATAL_ERROR "tile manifest is missing: ${expected_line}")
    endif()
endforeach()

file(SHA256 "${output_manifest}" first_manifest_hash)
file(SHA256 "${output_tile_manifest}" first_tile_manifest_hash)

set(repeat_root "${test_root}-repeat")
file(REMOVE_RECURSE "${repeat_root}")
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
file(SHA256 "${repeat_root}/PointCloud_payloads/tiles.manifest"
     repeat_tile_manifest_hash)
if(NOT first_tile_manifest_hash STREQUAL repeat_tile_manifest_hash)
    message(FATAL_ERROR "tile manifest is not deterministic")
endif()

set(cache_root "${test_root}/cache")
set(cache_first_root "${test_root}-cache-first")
set(cache_second_root "${test_root}-cache-second")
file(REMOVE_RECURSE "${cache_first_root}" "${cache_second_root}")
file(MAKE_DIRECTORY "${cache_first_root}" "${cache_second_root}")
execute_process(
    COMMAND "${converter}" "${fixture}" "${cache_first_root}/PointCloud.usda"
            --tile-size 1 --memory-limit 1024
            --attributes xyz,intensity
            --cache-root "${cache_root}"
    RESULT_VARIABLE cache_first_result
    OUTPUT_VARIABLE cache_first_output
    ERROR_VARIABLE cache_first_error)
string(FIND "${cache_first_output}"
    "Cache lookups: 1, hits: 0, misses: 1" cache_first_stats_position)
if(NOT cache_first_result EQUAL 0 OR NOT cache_first_error STREQUAL "" OR
   cache_first_stats_position EQUAL -1)
    message(FATAL_ERROR
        "cached first conversion failed: ${cache_first_output}${cache_first_error}")
endif()
# Cache entries are two levels deep: <generation key>/<source identity key>.
file(GLOB cache_entries RELATIVE "${cache_root}" "${cache_root}/*/*")
list(LENGTH cache_entries cache_entry_count)
if(NOT cache_entry_count EQUAL 1 OR
   NOT EXISTS "${cache_root}/${cache_entries}/root.usdc" OR
   NOT EXISTS "${cache_root}/${cache_entries}/cache.manifest" OR
    NOT EXISTS "${cache_root}/${cache_entries}/payloads" OR
    NOT EXISTS "${cache_root}/${cache_entries}/payloads/tiles.manifest")
    message(FATAL_ERROR "converter did not publish the expected cache entry")
endif()

execute_process(
    COMMAND "${converter}" "${fixture}" "${cache_second_root}/PointCloud.usda"
            --tile-size 1 --memory-limit 1024
            --attributes intensity,xyz
            --cache-root "${cache_root}"
    RESULT_VARIABLE cache_second_result
    OUTPUT_VARIABLE cache_second_output
    ERROR_VARIABLE cache_second_error)
string(FIND "${cache_second_output}" "Cache hit " cache_hit_position)
string(FIND "${cache_second_output}"
    "Cache lookups: 1, hits: 1, misses: 0" cache_hit_stats_position)
if(NOT cache_second_result EQUAL 0 OR NOT cache_second_error STREQUAL "" OR
   cache_hit_position EQUAL -1 OR
   cache_hit_stats_position EQUAL -1 OR
   NOT EXISTS "${cache_second_root}/PointCloud.usda" OR
   NOT EXISTS "${cache_second_root}/PointCloud.usda.manifest" OR
    NOT EXISTS "${cache_second_root}/payloads" OR
    NOT EXISTS "${cache_second_root}/payloads/tiles.manifest")
    message(FATAL_ERROR
        "converter did not reuse the cache entry: "
        "${cache_second_output}${cache_second_error}")
endif()

# No credential, signed URL, resolved identifier, or validation token is ever
# persisted into a manifest or a cache entry. The local path is the strongest
# identifier this fixture can produce and the fnv1a64 token is its validation
# material; neither may appear in any published artifact.
foreach(secret_manifest
        "${cache_root}/${cache_entries}/cache.manifest"
        "${cache_root}/${cache_entries}/payloads/tiles.manifest"
        "${cache_second_root}/PointCloud.usda.manifest"
        "${cache_second_root}/payloads/tiles.manifest")
    file(READ "${secret_manifest}" secret_manifest_content)
    string(FIND "${secret_manifest_content}" "fnv1a64:" secret_token_position)
    get_filename_component(fixture_directory "${fixture}" DIRECTORY)
    string(FIND "${secret_manifest_content}" "${fixture_directory}"
        secret_path_position)
    string(FIND "${secret_manifest_content}" "${cache_root}"
        secret_cache_position)
    if(NOT secret_token_position EQUAL -1 OR
       NOT secret_path_position EQUAL -1 OR
       NOT secret_cache_position EQUAL -1)
        message(FATAL_ERROR
            "manifest persisted source identity material: ${secret_manifest}")
    endif()
endforeach()

set(cache_rebuild_root "${test_root}-cache-rebuild")
set(cache_rebuild_output "${cache_rebuild_root}/PointCloud.usda")
file(REMOVE_RECURSE "${cache_rebuild_root}")
file(MAKE_DIRECTORY "${cache_rebuild_root}")
file(REMOVE "${cache_root}/${cache_entries}/payloads/tiles.manifest")
execute_process(
    COMMAND "${converter}" "${fixture}" "${cache_rebuild_output}"
            --tile-size 1 --memory-limit 1024
            --attributes intensity,xyz
            --cache-root "${cache_root}"
    RESULT_VARIABLE cache_rebuild_result
    OUTPUT_VARIABLE cache_rebuild_output_log
    ERROR_VARIABLE cache_rebuild_error_log)
string(FIND "${cache_rebuild_output_log}" "Cache hit " cache_rebuild_hit)
if(NOT cache_rebuild_result EQUAL 0 OR
   cache_rebuild_hit GREATER -1 OR
   NOT EXISTS "${cache_rebuild_output}" OR
   NOT EXISTS "${cache_rebuild_root}/payloads/tiles.manifest")
    message(FATAL_ERROR
        "converter did not rebuild a cache entry missing its tile manifest: "
        "${cache_rebuild_output_log}${cache_rebuild_error_log}")
endif()
    execute_process(
        COMMAND usdcat --flatten "${cache_second_root}/PointCloud.usda"
            -o "${cache_second_root}/flattened.usda"
        RESULT_VARIABLE cache_usdcat_result
        OUTPUT_VARIABLE cache_usdcat_output
        ERROR_VARIABLE cache_usdcat_error)
    if(NOT cache_usdcat_result EQUAL 0)
        message(FATAL_ERROR
        "materialized cache output could not be reopened: "
        "${cache_usdcat_output}${cache_usdcat_error}")
    endif()

    set(cache_recovery_root "${test_root}-cache-recovery")
    set(cache_recovery_output "${cache_recovery_root}/PointCloud.usda")
    set(cache_recovery_payloads "${cache_recovery_root}/payloads")
    set(cache_recovery_transaction "${cache_recovery_output}.transaction")
    file(REMOVE_RECURSE "${cache_recovery_root}")
    file(MAKE_DIRECTORY "${cache_recovery_transaction}" "${cache_recovery_payloads}")
    file(WRITE "${cache_recovery_output}.manifest" "stale manifest")
    file(WRITE "${cache_recovery_root}/PointCloud.tmp.usda" "stale root")
    file(WRITE "${cache_recovery_output}.manifest.tmp" "stale temporary manifest")
    file(WRITE "${cache_recovery_payloads}/stale.usdc" "stale payload")
    file(WRITE "${cache_recovery_transaction}/state"
        "payloadDirectory=${cache_recovery_payloads}\n")
    execute_process(
        COMMAND "${converter}" "${fixture}" "${cache_recovery_output}"
                --tile-size 1 --memory-limit 1024
                --attributes intensity,xyz
                --cache-root "${cache_root}"
        RESULT_VARIABLE cache_recovery_result
        OUTPUT_VARIABLE cache_recovery_output_log
        ERROR_VARIABLE cache_recovery_error_log)
    string(FIND "${cache_recovery_output_log}" "Cache hit " cache_recovery_hit)
    if(NOT cache_recovery_result EQUAL 0 OR
       cache_recovery_hit EQUAL -1 OR
       NOT EXISTS "${cache_recovery_output}" OR
       NOT EXISTS "${cache_recovery_output}.manifest" OR
       NOT EXISTS "${cache_recovery_payloads}" OR
       EXISTS "${cache_recovery_transaction}" OR
       EXISTS "${cache_recovery_root}/PointCloud.tmp.usda" OR
       EXISTS "${cache_recovery_output}.manifest.tmp")
        message(FATAL_ERROR
            "converter did not recover and reuse a cache hit: "
            "${cache_recovery_output_log}${cache_recovery_error_log}")
    endif()

set(failure_root "${test_root}-failure")
set(failure_output "${failure_root}/PointCloud.usda")
set(failure_payloads "${failure_root}/PointCloud_payloads")
file(REMOVE_RECURSE "${failure_root}")
file(MAKE_DIRECTORY "${failure_root}")
execute_process(
    COMMAND "${converter}" "${fixture}" "${failure_output}"
            --tile-size 1 --memory-limit 1024
            --attributes xyz,missing_attribute
    RESULT_VARIABLE failure_result
    OUTPUT_VARIABLE failure_output_log
    ERROR_VARIABLE failure_error_log)
if(failure_result EQUAL 0 OR EXISTS "${failure_output}" OR
   EXISTS "${failure_output}.manifest" OR
   EXISTS "${failure_output}.transaction" OR
   EXISTS "${failure_root}/PointCloud.tmp.usda" OR
   EXISTS "${failure_output}.manifest.tmp" OR
   EXISTS "${failure_payloads}")
    message(FATAL_ERROR
        "converter did not clean up after an authoring failure: "
        "${failure_output_log}${failure_error_log}")
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

set(unsafe_root "${test_root}-unsafe")
set(unsafe_output "${unsafe_root}/PointCloud.usda")
set(unsafe_payloads "${test_root}/ProtectedPayloads")
set(unsafe_transaction "${unsafe_output}.transaction")
file(REMOVE_RECURSE "${unsafe_root}")
file(MAKE_DIRECTORY "${unsafe_root}" "${unsafe_payloads}" "${unsafe_transaction}")
file(WRITE "${unsafe_output}.manifest" "stale manifest")
file(WRITE "${unsafe_root}/PointCloud.tmp.usda" "stale root")
file(WRITE "${unsafe_payloads}/must-survive.usdc" "protected payload")
file(WRITE "${unsafe_transaction}/state"
    "payloadDirectory=${unsafe_payloads}\n")

execute_process(
    COMMAND "${converter}" "${fixture}" "${unsafe_output}"
            --tile-size 1 --memory-limit 1024
            --attributes xyz,intensity
    RESULT_VARIABLE unsafe_result
    OUTPUT_VARIABLE unsafe_output_log
    ERROR_VARIABLE unsafe_error_log)
if(unsafe_result EQUAL 0 OR NOT EXISTS "${unsafe_payloads}/must-survive.usdc" OR
   NOT EXISTS "${unsafe_transaction}")
    message(FATAL_ERROR
        "converter removed or accepted an unsafe transaction payload path: "
        "${unsafe_output_log}${unsafe_error_log}")
endif()

set(orphan_root "${test_root}-orphan")
set(orphan_output "${orphan_root}/PointCloud.usda")
set(orphan_payloads "${orphan_root}/PointCloud_payloads")
set(orphan_transaction "${orphan_output}.transaction")
file(REMOVE_RECURSE "${orphan_root}")
file(MAKE_DIRECTORY "${orphan_root}" "${orphan_payloads}" "${orphan_transaction}")
file(WRITE "${orphan_output}.manifest" "stale manifest")
file(WRITE "${orphan_root}/PointCloud.tmp.usda" "stale root")
file(WRITE "${orphan_output}.manifest.tmp" "stale temporary manifest")
file(WRITE "${orphan_payloads}/stale.usdc" "stale payload")

execute_process(
    COMMAND "${converter}" "${fixture}" "${orphan_output}"
            --tile-size 1 --memory-limit 1024
            --attributes xyz,intensity
    RESULT_VARIABLE orphan_result
    OUTPUT_VARIABLE orphan_output_log
    ERROR_VARIABLE orphan_error_log)
if(NOT orphan_result EQUAL 0 OR NOT EXISTS "${orphan_output}" OR
   NOT EXISTS "${orphan_payloads}" OR NOT EXISTS "${orphan_output}.manifest" OR
   EXISTS "${orphan_transaction}" OR EXISTS "${orphan_root}/PointCloud.tmp.usda" OR
   EXISTS "${orphan_output}.manifest.tmp")
    message(FATAL_ERROR
        "converter did not recover a transaction without state: "
        "${orphan_output_log}${orphan_error_log}")
endif()

file(REMOVE_RECURSE "${test_root}")
file(REMOVE_RECURSE "${repeat_root}")
file(REMOVE_RECURSE "${cache_first_root}")
file(REMOVE_RECURSE "${cache_second_root}")
file(REMOVE_RECURSE "${cache_rebuild_root}")
file(REMOVE_RECURSE "${cache_recovery_root}")
file(REMOVE_RECURSE "${failure_root}")
file(REMOVE_RECURSE "${unsafe_root}")
file(REMOVE_RECURSE "${orphan_root}")