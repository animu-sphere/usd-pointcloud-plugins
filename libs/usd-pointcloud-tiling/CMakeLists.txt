cmake_minimum_required(VERSION 3.23)

project(usdPointCloudTiling
    VERSION 0.1.0
    DESCRIPTION "Format-independent point-cloud spatial tiling contracts"
    LANGUAGES CXX)

add_library(usdPointCloudTiling STATIC src/Tiling.cpp)
add_library(usdpointcloud::tiling ALIAS usdPointCloudTiling)
set_target_properties(usdPointCloudTiling PROPERTIES
    EXPORT_NAME tiling
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(usdPointCloudTiling PUBLIC cxx_std_17)
target_link_libraries(usdPointCloudTiling PUBLIC usdgeo::core usdpointcloud::core)
target_include_directories(usdPointCloudTiling
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

if(MSVC)
    target_compile_options(usdPointCloudTiling PRIVATE /utf-8)
endif()

include(GNUInstallDirs)
install(TARGETS usdPointCloudTiling
    EXPORT usdPointCloudTilingTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(USDGEO_BUILD_TESTS)
    add_subdirectory(tests)
endif()
