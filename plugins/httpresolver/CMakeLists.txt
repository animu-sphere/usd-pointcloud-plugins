cmake_minimum_required(VERSION 3.23)

project(httpResolver
    VERSION 0.9.0
    DESCRIPTION "OpenUSD HTTP resolver test double for point-cloud integration"
    LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/../pointcloud-las/cmake/OpenStrataPlugin.cmake")
openstrata_default_build_type()

if(NOT TARGET OpenGL::GL)
    find_package(OpenGL REQUIRED)
endif()
if(NOT TARGET Vulkan::Vulkan)
    find_package(Vulkan QUIET COMPONENTS shaderc_combined)
endif()
if(NOT TARGET Vulkan::Vulkan)
    add_library(Vulkan::Vulkan INTERFACE IMPORTED)
endif()
if(NOT TARGET Vulkan::shaderc_combined)
    add_library(Vulkan::shaderc_combined INTERFACE IMPORTED)
endif()
if(NOT pxr_FOUND)
    find_package(pxr CONFIG REQUIRED)
endif()

set(PLUGIN_NAME HttpResolver)
add_library(${PLUGIN_NAME} SHARED src/HttpResolver.cpp)

target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_17)
target_include_directories(${PLUGIN_NAME} PRIVATE ${PXR_INCLUDE_DIRS})
if(MSVC)
    target_compile_definitions(${PLUGIN_NAME} PRIVATE NOMINMAX)
    target_compile_options(${PLUGIN_NAME} PRIVATE /utf-8)
endif()

openstrata_link_openusd(
    TARGET ${PLUGIN_NAME}
    COMPONENTS arch tf ar)

openstrata_configure_plugin(
    TARGET ${PLUGIN_NAME}
    PLUG_INFO_INPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/httpresolver/plugInfo.json.in"
    PLUG_INFO_OUTPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/httpresolver/plugInfo.json")

openstrata_install_plugin_bundle(
    TARGET ${PLUGIN_NAME}
    RESOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/httpresolver/plugInfo.json"
    RESOURCE_DESTINATION "plugin/resources/httpresolver"
    MANIFESTS
        "${CMAKE_CURRENT_SOURCE_DIR}/openstrata.plugin.yaml")