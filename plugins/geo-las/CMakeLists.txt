cmake_minimum_required(VERSION 3.23)

project(geoLas
    VERSION 0.1.0
    DESCRIPTION "OpenUSD FileFormat Plugin for LAS point clouds"
    LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/OpenStrataPlugin.cmake")
openstrata_default_build_type()

if(NOT pxr_FOUND)
    find_package(pxr CONFIG REQUIRED)
endif()

set(PLUGIN_NAME GeoLasFileFormat)
add_library(${PLUGIN_NAME} SHARED src/GeoLasFileFormat.cpp)

target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_17)
target_link_libraries(${PLUGIN_NAME} PRIVATE usdlas::core usdgeo::usd)
target_include_directories(${PLUGIN_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${PXR_INCLUDE_DIRS})

openstrata_link_openusd(
    TARGET ${PLUGIN_NAME}
    COMPONENTS arch tf gf vt sdf usd)

if(USDGEO_BUILD_TESTS)
    add_executable(geoLas_tests tests/test_geo_las.cpp)
    target_compile_features(geoLas_tests PRIVATE cxx_std_17)
    target_include_directories(geoLas_tests PRIVATE ${PXR_INCLUDE_DIRS})
    target_compile_definitions(geoLas_tests PRIVATE
        GEOLAS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_dependencies(geoLas_tests ${PLUGIN_NAME})
    if(MSVC)
        target_compile_definitions(geoLas_tests PRIVATE NOMINMAX)
        target_compile_options(geoLas_tests PRIVATE /utf-8)
    endif()
    openstrata_link_openusd(
        TARGET geoLas_tests
        COMPONENTS arch tf gf vt sdf usd usdGeom)
    set_target_properties(geoLas_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
    add_test(NAME geoLas_integration COMMAND geoLas_tests)
endif()

openstrata_configure_plugin(
    TARGET ${PLUGIN_NAME}
    PLUG_INFO_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/geo-las/plugInfo.json.in"
    PLUG_INFO_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/geo-las/plugInfo.json")

openstrata_install_plugin_bundle(
    TARGET ${PLUGIN_NAME}
    RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/geo-las/plugInfo.json"
    RESOURCE_DESTINATION "plugin/resources/geo-las"
    MANIFESTS
        "${CMAKE_CURRENT_SOURCE_DIR}/openstrata.plugin.yaml"
        "${CMAKE_CURRENT_SOURCE_DIR}/openstrata.scaffold.yaml")
