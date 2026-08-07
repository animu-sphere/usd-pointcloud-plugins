add_executable(usdCopc_tests test_copc.cpp)
target_include_directories(usdCopc_tests PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party/laz-perf/cpp")
target_link_libraries(usdCopc_tests PRIVATE usdcopc::core)
add_test(NAME usdCopc_unit COMMAND usdCopc_tests)
