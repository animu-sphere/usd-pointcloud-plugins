cmake_minimum_required(VERSION 3.23)

project(usdGeoCache
    VERSION 0.1.0
    DESCRIPTION "Deterministic OpenUSD point-cloud cache contracts"
    LANGUAGES CXX)

add_library(usdGeoCache STATIC src/Cache.cpp)
add_library(usdgeo::cache ALIAS usdGeoCache)
set_target_properties(usdGeoCache PROPERTIES
    EXPORT_NAME cache
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(usdGeoCache PUBLIC cxx_std_17)
target_link_libraries(usdGeoCache PUBLIC usdgeo::core)
target_include_directories(usdGeoCache
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

if(MSVC)
    target_compile_options(usdGeoCache PRIVATE /utf-8)
endif()

include(GNUInstallDirs)
install(TARGETS usdGeoCache
    EXPORT usdGeoCacheTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(USDGEO_BUILD_TESTS)
    add_subdirectory(tests)
endif()