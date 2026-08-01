cmake_minimum_required(VERSION 3.23)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
     _usd_geo_version LIMIT_COUNT 1)
string(STRIP "${_usd_geo_version}" _usd_geo_version)

project(usd-geo-plugins
    VERSION ${_usd_geo_version}
    DESCRIPTION "OpenUSD plugins for geospatial and surveying data"
    LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USDGEO_BUILD_TESTS "Build usd-geo-plugins tests" ON)
if(USDGEO_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(libs/usd-geo-core)
add_subdirectory(libs/usd-pointcloud-core)
add_subdirectory(libs/usd-geo-usd)
