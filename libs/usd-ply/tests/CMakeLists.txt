add_executable(usdPly_tests test_ply.cpp)
target_link_libraries(usdPly_tests PRIVATE usdply::core)
add_test(NAME usdPly_unit COMMAND usdPly_tests)