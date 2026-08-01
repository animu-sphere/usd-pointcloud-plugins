cmake_minimum_required(VERSION 3.23)

project(usdPointCloudCore
    VERSION 0.1.0
    DESCRIPTION "Format-independent point-cloud contracts"
    LANGUAGES CXX)

add_library(usdPointCloudCore STATIC
    src/FileFormatArguments.cpp
    src/PointCloud.cpp)
add_library(usdpointcloud::core ALIAS usdPointCloudCore)
set_target_properties(usdPointCloudCore PROPERTIES
    EXPORT_NAME core
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(usdPointCloudCore PUBLIC cxx_std_17)
target_link_libraries(usdPointCloudCore PUBLIC usdgeo::core)
target_include_directories(usdPointCloudCore
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

if(MSVC)
    target_compile_options(usdPointCloudCore PRIVATE /utf-8)
endif()

include(GNUInstallDirs)
install(TARGETS usdPointCloudCore
    EXPORT usdPointCloudCoreTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(USDGEO_BUILD_TESTS)
    add_subdirectory(tests)
endif()