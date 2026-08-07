cmake_minimum_required(VERSION 3.23)

project(pointcloudCopc
    VERSION 0.3.0
    DESCRIPTION "OpenUSD FileFormat Plugin for COPC point clouds"
    LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/../pointcloud-las/cmake/OpenStrataPlugin.cmake")
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
if(NOT TARGET usdlas::core)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-las"
        "${CMAKE_CURRENT_BINARY_DIR}/usd-las")
endif()
if(NOT TARGET usdlaz::core)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-laz"
        "${CMAKE_CURRENT_BINARY_DIR}/usd-laz")
endif()
if(NOT TARGET usdcopc::core)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-copc"
        "${CMAKE_CURRENT_BINARY_DIR}/usd-copc")
endif()
if(NOT TARGET OpenGL::GL)
    find_package(OpenGL REQUIRED)
endif()
if(NOT TARGET usdpointcloud::tiling)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-pointcloud-tiling"
        "${CMAKE_CURRENT_BINARY_DIR}/usd-pointcloud-tiling")
endif()
if(NOT TARGET usdpointcloud::authoring)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-pointcloud-authoring"
        "${CMAKE_CURRENT_BINARY_DIR}/usd-pointcloud-authoring")
endif()

if(NOT pxr_FOUND)
    find_package(pxr CONFIG REQUIRED)
endif()

set(PLUGIN_NAME UsdGeoCopcFileFormat)
add_library(${PLUGIN_NAME} SHARED src/UsdGeoCopcFileFormat.cpp)

target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_17)
target_link_libraries(${PLUGIN_NAME} PRIVATE usdcopc::core usdpointcloud::authoring)
target_include_directories(${PLUGIN_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${PXR_INCLUDE_DIRS})

openstrata_link_openusd(
    TARGET ${PLUGIN_NAME}
    COMPONENTS arch tf gf vt sdf usd)

if(USDGEO_BUILD_TESTS)
    add_executable(pointcloudCopc_tests tests/test_pointcloud_copc.cpp)
    target_compile_features(pointcloudCopc_tests PRIVATE cxx_std_17)
    target_compile_definitions(pointcloudCopc_tests PRIVATE
        USDGEOCOPC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        USDGEOLAS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../pointcloud-las"
        USDGEOLAZ_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../pointcloud-laz")
    target_include_directories(pointcloudCopc_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/laz-perf/cpp"
        ${PXR_INCLUDE_DIRS})
    add_dependencies(pointcloudCopc_tests ${PLUGIN_NAME})
    if(TARGET UsdGeoLasFileFormat)
        add_dependencies(pointcloudCopc_tests UsdGeoLasFileFormat)
    endif()
    if(TARGET UsdGeoLazFileFormat)
        add_dependencies(pointcloudCopc_tests UsdGeoLazFileFormat)
    endif()
    if(MSVC)
        target_compile_definitions(pointcloudCopc_tests PRIVATE NOMINMAX)
        target_compile_options(pointcloudCopc_tests PRIVATE /utf-8)
    endif()
    openstrata_link_openusd(
        TARGET pointcloudCopc_tests
        COMPONENTS arch tf gf vt sdf usd usdGeom usdLod)
    set_target_properties(pointcloudCopc_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
    add_test(NAME pointcloudCopc_integration COMMAND pointcloudCopc_tests)
endif()

openstrata_configure_plugin(
    TARGET ${PLUGIN_NAME}
    PLUG_INFO_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/pointcloud-copc/plugInfo.json.in"
    PLUG_INFO_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/pointcloud-copc/plugInfo.json")

openstrata_install_plugin_bundle(
    TARGET ${PLUGIN_NAME}
    RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/pointcloud-copc/plugInfo.json"
    RESOURCE_DESTINATION "plugin/resources/pointcloud-copc"
    MANIFESTS
        "${CMAKE_CURRENT_SOURCE_DIR}/openstrata.plugin.yaml")
