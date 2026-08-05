add_executable(usdCopc_tests test_copc.cpp)
target_link_libraries(usdCopc_tests PRIVATE usdcopc::core)
add_test(NAME usdCopc_unit COMMAND usdCopc_tests)
