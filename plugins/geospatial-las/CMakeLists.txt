cmake_minimum_required(VERSION 3.23)

project(geospatialLas
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

if(NOT TARGET usdgeo::core)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-geo-core"
        "${CMAKE_CURRENT_BINARY_DIR}/usd-geo-core")
endif()
if(NOT TARGET usdpointcloud::core)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-pointcloud-core"
        "${CMAKE_CURRENT_BINARY_DIR}/usd-pointcloud-core")
endif()
if(NOT TARGET OpenGL::GL)
    find_package(OpenGL REQUIRED)
endif()
if(NOT TARGET usdpointcloud::authoring)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-pointcloud-authoring"
        "${CMAKE_CURRENT_BINARY_DIR}/usd-pointcloud-authoring")
endif()
if(NOT TARGET usdlas::core)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-las"
        "${CMAKE_CURRENT_BINARY_DIR}/usd-las")
endif()

if(NOT pxr_FOUND)
    find_package(pxr CONFIG REQUIRED)
endif()

set(PLUGIN_NAME UsdGeoLasFileFormat)
add_library(${PLUGIN_NAME} SHARED src/UsdGeoLasFileFormat.cpp)

target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_17)
target_link_libraries(${PLUGIN_NAME} PRIVATE usdlas::core usdpointcloud::authoring)
target_include_directories(${PLUGIN_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${PXR_INCLUDE_DIRS})

openstrata_link_openusd(
    TARGET ${PLUGIN_NAME}
    COMPONENTS arch tf gf vt sdf usd)

if(USDGEO_BUILD_TESTS)
    add_executable(geospatialLas_tests tests/test_geospatial_las.cpp)
    target_compile_features(geospatialLas_tests PRIVATE cxx_std_17)
    target_include_directories(geospatialLas_tests PRIVATE ${PXR_INCLUDE_DIRS})
    target_compile_definitions(geospatialLas_tests PRIVATE
        USDGEOLAS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_dependencies(geospatialLas_tests ${PLUGIN_NAME})
    if(MSVC)
        target_compile_definitions(geospatialLas_tests PRIVATE NOMINMAX)
        target_compile_options(geospatialLas_tests PRIVATE /utf-8)
    endif()
    openstrata_link_openusd(
        TARGET geospatialLas_tests
        COMPONENTS arch tf gf vt sdf usd usdGeom)
    set_target_properties(geospatialLas_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
    add_test(NAME geospatialLas_integration COMMAND geospatialLas_tests)
endif()

openstrata_configure_plugin(
    TARGET ${PLUGIN_NAME}
    PLUG_INFO_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/geospatial-las/plugInfo.json.in"
    PLUG_INFO_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/geospatial-las/plugInfo.json")

openstrata_install_plugin_bundle(
    TARGET ${PLUGIN_NAME}
    RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/geospatial-las/plugInfo.json"
    RESOURCE_DESTINATION "plugin/resources/geospatial-las"
    MANIFESTS
        "${CMAKE_CURRENT_SOURCE_DIR}/openstrata.plugin.yaml"
        "${CMAKE_CURRENT_SOURCE_DIR}/openstrata.scaffold.yaml")
