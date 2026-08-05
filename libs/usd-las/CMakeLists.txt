cmake_minimum_required(VERSION 3.23)

project(usdLas
    VERSION 0.1.0
    DESCRIPTION "LAS metadata inspection and point-record decoding"
    LANGUAGES CXX)

add_library(usdLas STATIC
    src/LasFormat.cpp
    src/LasMetadata.cpp
    src/LasPoint.cpp
    src/LasPointCloud.cpp
    src/LasReader.cpp
    src/LasStream.cpp)
add_library(usdlas::core ALIAS usdLas)
set_target_properties(usdLas PROPERTIES
    EXPORT_NAME core
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(usdLas PUBLIC cxx_std_17)
target_link_libraries(usdLas PUBLIC usdgeo::core usdpointcloud::core)
target_include_directories(usdLas
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

if(MSVC)
    target_compile_options(usdLas PRIVATE /utf-8)
endif()

include(GNUInstallDirs)
install(TARGETS usdLas
    EXPORT usdLasTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(USDGEO_BUILD_TESTS)
    add_subdirectory(tests)
endif()