cmake_minimum_required(VERSION 3.23)

project(usdGeoCore
    VERSION 0.1.0
    DESCRIPTION "Format-independent geospatial metadata and spatial primitives"
    LANGUAGES CXX)

add_library(usdGeoCore STATIC
    src/CacheKey.cpp
    src/GeoReference.cpp
    src/SpatialBounds.cpp
    src/TileId.cpp)
add_library(usdgeo::core ALIAS usdGeoCore)
set_target_properties(usdGeoCore PROPERTIES
    EXPORT_NAME core
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(usdGeoCore PUBLIC cxx_std_17)
target_include_directories(usdGeoCore
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

if(MSVC)
    target_compile_options(usdGeoCore PRIVATE /utf-8)
endif()

include(GNUInstallDirs)
install(TARGETS usdGeoCore
    EXPORT usdGeoCoreTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(USDGEO_BUILD_TESTS)
    add_subdirectory(tests)
endif()
