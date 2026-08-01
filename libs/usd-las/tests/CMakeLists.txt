add_executable(usdLas_tests test_las.cpp)
target_link_libraries(usdLas_tests PRIVATE usdlas::core)
add_test(NAME usdLas_unit COMMAND usdLas_tests)