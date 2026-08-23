cmake_minimum_required(VERSION 3.23)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
     _usd_geo_version LIMIT_COUNT 1)
string(STRIP "${_usd_geo_version}" _usd_geo_version)

project(usd-pointcloud-plugins
    VERSION ${_usd_geo_version}
    DESCRIPTION "OpenUSD file format plugins and libraries for point cloud data"
    LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USDGEO_BUILD_TESTS "Build usd-pointcloud-plugins tests" ON)
if(USDGEO_BUILD_TESTS)
    enable_testing()
endif()
option(USDGEO_BUILD_BENCHMARKS
       "Build explicit OpenUSD streaming benchmark executables" OFF)

function(usdgeo_configure_openusd_test_runtime test_name)
    if(WIN32)
        set_property(TEST "${test_name}" APPEND PROPERTY
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:usdGeom>")
    endif()
endfunction()

set(_usdgeo_build_usd_default OFF)
if(DEFINED pxr_ROOT OR DEFINED OpenUSD_ROOT)
    set(_usdgeo_build_usd_default ON)
endif()
option(USDGEO_BUILD_USD "Build the OpenUSD authoring bridge" ${_usdgeo_build_usd_default})

add_subdirectory(libs/usd-geo-core)
add_subdirectory(libs/usd-geo-cache)
add_subdirectory(libs/usd-pointcloud-core)
add_subdirectory(libs/usd-pointcloud-tiling)
add_subdirectory(libs/usd-las)
add_subdirectory(libs/usd-laz)
add_subdirectory(libs/usd-copc)
add_subdirectory(libs/usd-ply)
if(USDGEO_BUILD_USD)
    find_package(OpenGL REQUIRED)
    add_subdirectory(libs/usd-pointcloud-authoring)
    add_subdirectory(tools/usd-pointcloud-convert)
    add_subdirectory(plugins/pointcloud-las)
    add_subdirectory(plugins/pointcloud-laz)
    add_subdirectory(plugins/pointcloud-copc)
    add_subdirectory(plugins/pointcloud-ply)
endif()
