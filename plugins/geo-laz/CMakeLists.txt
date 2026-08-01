cmake_minimum_required(VERSION 3.23)

project(geoLaz
    VERSION 0.1.0
    DESCRIPTION "OpenUSD FileFormat Plugin for LAZ point clouds"
    LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/../geo-las/cmake/OpenStrataPlugin.cmake")
openstrata_default_build_type()

if(NOT pxr_FOUND)
    find_package(pxr CONFIG REQUIRED)
endif()

set(PLUGIN_NAME GeoLazFileFormat)
add_library(${PLUGIN_NAME} SHARED src/GeoLazFileFormat.cpp)

target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_17)
target_link_libraries(${PLUGIN_NAME} PRIVATE usdlaz::core usdgeo::usd)
target_include_directories(${PLUGIN_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${PXR_INCLUDE_DIRS})

openstrata_link_openusd(
    TARGET ${PLUGIN_NAME}
    COMPONENTS arch tf gf vt sdf usd)

if(USDGEO_BUILD_TESTS)
    add_executable(geoLaz_tests tests/test_geo_laz.cpp)
    target_compile_features(geoLaz_tests PRIVATE cxx_std_17)
    target_link_libraries(geoLaz_tests PRIVATE usdlaz::core)
    target_include_directories(geoLaz_tests PRIVATE
        ${PXR_INCLUDE_DIRS}
        "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/laz-perf/cpp")
    target_compile_definitions(geoLaz_tests PRIVATE
        GEOLAZ_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_dependencies(geoLaz_tests ${PLUGIN_NAME})
    if(MSVC)
        target_compile_definitions(geoLaz_tests PRIVATE NOMINMAX)
        target_compile_options(geoLaz_tests PRIVATE /utf-8)
    endif()
    openstrata_link_openusd(
        TARGET geoLaz_tests
        COMPONENTS arch tf gf vt sdf usd usdGeom)
    set_target_properties(geoLaz_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
    add_test(NAME geoLaz_integration COMMAND geoLaz_tests)
endif()

openstrata_configure_plugin(
    TARGET ${PLUGIN_NAME}
    PLUG_INFO_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/geo-laz/plugInfo.json.in"
    PLUG_INFO_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/geo-laz/plugInfo.json")

openstrata_install_plugin_bundle(
    TARGET ${PLUGIN_NAME}
    RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/geo-laz/plugInfo.json"
    RESOURCE_DESTINATION "plugin/resources/geo-laz"
    MANIFESTS
        "${CMAKE_CURRENT_SOURCE_DIR}/openstrata.plugin.yaml")
