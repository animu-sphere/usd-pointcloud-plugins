cmake_minimum_required(VERSION 3.23)

project(usdPointCloudConvert
    VERSION 0.1.0
    DESCRIPTION "Explicit LAS and LAZ to tiled USD conversion tool"
    LANGUAGES CXX)

find_package(pxr CONFIG REQUIRED)

add_executable(usd-pointcloud-convert main.cpp)
target_compile_features(usd-pointcloud-convert PRIVATE cxx_std_17)
target_link_libraries(usd-pointcloud-convert
    PRIVATE
        usdpointcloud::authoring
        usdlas::core
        usdlaz::core)
target_include_directories(usd-pointcloud-convert PRIVATE
    "${CMAKE_SOURCE_DIR}/libs/usd-geo-core/include"
    "${CMAKE_SOURCE_DIR}/libs/usd-pointcloud-core/include"
    "${CMAKE_SOURCE_DIR}/libs/usd-pointcloud-authoring/include"
    "${CMAKE_SOURCE_DIR}/libs/usd-las/include"
    "${CMAKE_SOURCE_DIR}/libs/usd-laz/include")

if(MSVC)
    target_compile_definitions(usd-pointcloud-convert PRIVATE NOMINMAX)
    target_compile_options(usd-pointcloud-convert PRIVATE /utf-8)
endif()

include(GNUInstallDirs)
install(TARGETS usd-pointcloud-convert
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(USDGEO_BUILD_TESTS)
    add_test(NAME usdPointCloudConvert_help
             COMMAND usd-pointcloud-convert --help)
    add_test(NAME usdPointCloudConvert_las
             COMMAND ${CMAKE_COMMAND}
                     -Dconverter=$<TARGET_FILE:usd-pointcloud-convert>
                     -Dfixture=${CMAKE_SOURCE_DIR}/plugins/pointcloud-las/tests/fixtures/conformance.las
                     -Dtest_root=${CMAKE_CURRENT_BINARY_DIR}/converter-conformance
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/test_conversion.cmake)
endif()
