cmake_minimum_required(VERSION 3.23)

project(usdLaz
    VERSION 0.1.0
    DESCRIPTION "Chunked LAZ reader contract"
    LANGUAGES CXX)

set(LAZPERF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/laz-perf")
if(NOT EXISTS "${LAZPERF_ROOT}/cpp/lazperf/io.cpp")
    message(FATAL_ERROR "laz-perf source is missing: ${LAZPERF_ROOT}")
endif()

set(LAZPERF_SOURCES
    "${LAZPERF_ROOT}/cpp/lazperf/charbuf.cpp"
    "${LAZPERF_ROOT}/cpp/lazperf/filestream.cpp"
    "${LAZPERF_ROOT}/cpp/lazperf/io.cpp"
    "${LAZPERF_ROOT}/cpp/lazperf/lazperf.cpp"
    "${LAZPERF_ROOT}/cpp/lazperf/vlr.cpp"
    "${LAZPERF_ROOT}/cpp/lazperf/detail/field_byte10.cpp"
    "${LAZPERF_ROOT}/cpp/lazperf/detail/field_byte14.cpp"
    "${LAZPERF_ROOT}/cpp/lazperf/detail/field_gpstime10.cpp"
    "${LAZPERF_ROOT}/cpp/lazperf/detail/field_nir14.cpp"
    "${LAZPERF_ROOT}/cpp/lazperf/detail/field_point10.cpp"
    "${LAZPERF_ROOT}/cpp/lazperf/detail/field_point14.cpp"
    "${LAZPERF_ROOT}/cpp/lazperf/detail/field_rgb10.cpp"
    "${LAZPERF_ROOT}/cpp/lazperf/detail/field_rgb14.cpp")

add_library(usdLaz STATIC src/Laz.cpp src/LazPerfDecoder.cpp ${LAZPERF_SOURCES})
add_library(usdlaz::core ALIAS usdLaz)
set_target_properties(usdLaz PROPERTIES
    EXPORT_NAME core
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(usdLaz PUBLIC cxx_std_17)
target_link_libraries(usdLaz PUBLIC usdlas::core)
target_include_directories(usdLaz PRIVATE "${LAZPERF_ROOT}/cpp")
target_include_directories(usdLaz
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

if(MSVC)
    target_compile_options(usdLaz PRIVATE /utf-8)
endif()

if(USDGEO_BUILD_TESTS)
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)
install(TARGETS usdLaz
    EXPORT usdLazTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
