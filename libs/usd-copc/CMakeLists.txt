cmake_minimum_required(VERSION 3.23)

project(usdCopc
    VERSION 0.1.0
    DESCRIPTION "COPC metadata and hierarchy reader"
    LANGUAGES CXX)

add_library(usdCopc STATIC src/Copc.cpp)
add_library(usdcopc::core ALIAS usdCopc)
set_target_properties(usdCopc PROPERTIES
    EXPORT_NAME core
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(usdCopc PUBLIC cxx_std_17)
target_link_libraries(usdCopc PUBLIC
    usdlas::core
    usdlaz::core
    usdpointcloud::core
    usdpointcloud::tiling)
target_include_directories(usdCopc
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

if(MSVC)
    target_compile_options(usdCopc PRIVATE /utf-8)
endif()

include(GNUInstallDirs)
install(TARGETS usdCopc
    EXPORT usdCopcTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(USDGEO_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(USDGEO_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
