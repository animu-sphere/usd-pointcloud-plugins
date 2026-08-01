add_executable(usdGeoCore_tests test_core.cpp)
target_link_libraries(usdGeoCore_tests PRIVATE usdgeo::core)
add_test(NAME usdGeoCore_unit COMMAND usdGeoCore_tests)
