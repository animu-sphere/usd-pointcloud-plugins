cmake_minimum_required(VERSION 3.23)

project(pointcloudPly
    VERSION 0.6.0
    DESCRIPTION "OpenUSD FileFormat Plugin for PLY point clouds"
    LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/../pointcloud-las/cmake/OpenStrataPlugin.cmake")
openstrata_default_build_type()

if(NOT TARGET usdgeo::core)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-geo-core"
                     "${CMAKE_CURRENT_BINARY_DIR}/usd-geo-core")
endif()
if(NOT TARGET usdpointcloud::core)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-pointcloud-core"
                     "${CMAKE_CURRENT_BINARY_DIR}/usd-pointcloud-core")
endif()
if(NOT TARGET usdgeo::cache)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-geo-cache"
                     "${CMAKE_CURRENT_BINARY_DIR}/usd-geo-cache")
endif()
if(NOT TARGET usdpointcloud::tiling)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-pointcloud-tiling"
                     "${CMAKE_CURRENT_BINARY_DIR}/usd-pointcloud-tiling")
endif()
if(NOT TARGET OpenGL::GL)
    find_package(OpenGL REQUIRED)
endif()
if(NOT TARGET usdpointcloud::authoring)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-pointcloud-authoring"
                     "${CMAKE_CURRENT_BINARY_DIR}/usd-pointcloud-authoring")
endif()
if(NOT TARGET usdply::core)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-ply"
                     "${CMAKE_CURRENT_BINARY_DIR}/usd-ply")
endif()

if(NOT pxr_FOUND)
    find_package(pxr CONFIG REQUIRED)
endif()

set(PLUGIN_NAME UsdGeoPlyFileFormat)
add_library(${PLUGIN_NAME} SHARED src/UsdGeoPlyFileFormat.cpp)
target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_17)
target_link_libraries(${PLUGIN_NAME} PRIVATE usdply::core usdpointcloud::authoring)
target_include_directories(${PLUGIN_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${PXR_INCLUDE_DIRS})

openstrata_link_openusd(
    TARGET ${PLUGIN_NAME}
    COMPONENTS arch tf gf vt sdf pcp usd)

if(USDGEO_BUILD_TESTS)
    add_executable(pointcloudPly_tests tests/test_pointcloud_ply.cpp)
    target_compile_features(pointcloudPly_tests PRIVATE cxx_std_17)
    target_include_directories(pointcloudPly_tests PRIVATE ${PXR_INCLUDE_DIRS})
    target_compile_definitions(pointcloudPly_tests PRIVATE
        USDGEOPLY_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_dependencies(pointcloudPly_tests ${PLUGIN_NAME})
    if(MSVC)
        target_compile_definitions(pointcloudPly_tests PRIVATE NOMINMAX)
        target_compile_options(pointcloudPly_tests PRIVATE /utf-8)
    endif()
    openstrata_link_openusd(
        TARGET pointcloudPly_tests
        COMPONENTS arch tf gf vt sdf pcp usd usdGeom)
    set_target_properties(pointcloudPly_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
    add_test(NAME pointcloudPly_integration COMMAND pointcloudPly_tests)
endif()

openstrata_configure_plugin(
    TARGET ${PLUGIN_NAME}
    PLUG_INFO_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/pointcloud-ply/plugInfo.json.in"
    PLUG_INFO_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/pointcloud-ply/plugInfo.json")

openstrata_install_plugin_bundle(
    TARGET ${PLUGIN_NAME}
    RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/pointcloud-ply/plugInfo.json"
    RESOURCE_DESTINATION "plugin/resources/pointcloud-ply"
    MANIFESTS
        "${CMAKE_CURRENT_SOURCE_DIR}/openstrata.plugin.yaml")