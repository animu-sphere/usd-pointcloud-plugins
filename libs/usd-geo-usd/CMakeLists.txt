cmake_minimum_required(VERSION 3.23)

project(usdGeoUsd
    VERSION 0.1.0
    DESCRIPTION "OpenUSD authoring bridge for geospatial data"
    LANGUAGES CXX)

find_package(pxr CONFIG REQUIRED)

add_library(usdGeoUsd STATIC src/PointCloudLayer.cpp)
add_library(usdgeo::usd ALIAS usdGeoUsd)
set_target_properties(usdGeoUsd PROPERTIES
    EXPORT_NAME usd
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(usdGeoUsd PUBLIC cxx_std_17)
target_link_libraries(usdGeoUsd
    PUBLIC
        usdgeo::core
        usdpointcloud::core
        usdGeom
        usdLod)
target_include_directories(usdGeoUsd
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

if(MSVC)
    target_compile_definitions(usdGeoUsd PUBLIC NOMINMAX)
    target_compile_options(usdGeoUsd PRIVATE /utf-8)
endif()

include(GNUInstallDirs)
install(TARGETS usdGeoUsd
    EXPORT usdGeoUsdTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(USDGEO_BUILD_TESTS)
    add_subdirectory(tests)
endif()