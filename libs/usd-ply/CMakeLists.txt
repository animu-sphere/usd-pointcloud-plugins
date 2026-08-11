cmake_minimum_required(VERSION 3.23)

project(usdPly
    VERSION 0.1.0
    DESCRIPTION "PLY header inspection"
    LANGUAGES CXX)

add_library(usdPly STATIC src/Ply.cpp)
add_library(usdply::core ALIAS usdPly)
set_target_properties(usdPly PROPERTIES
    EXPORT_NAME core
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(usdPly PUBLIC cxx_std_17)
target_link_libraries(usdPly PUBLIC usdgeo::core)
target_include_directories(usdPly
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

if(MSVC)
    target_compile_options(usdPly PRIVATE /utf-8)
endif()

include(GNUInstallDirs)
install(TARGETS usdPly
    EXPORT usdPlyTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(USDGEO_BUILD_TESTS)
    add_subdirectory(tests)
endif()