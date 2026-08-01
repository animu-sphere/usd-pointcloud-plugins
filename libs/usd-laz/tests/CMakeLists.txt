add_executable(usdLaz_tests test_laz.cpp)
target_compile_features(usdLaz_tests PRIVATE cxx_std_17)
target_link_libraries(usdLaz_tests PRIVATE usdlaz::core)
add_test(NAME usdLaz_unit COMMAND usdLaz_tests)
