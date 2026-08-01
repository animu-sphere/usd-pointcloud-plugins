add_executable(usdGeoUsd_tests test_usd_authoring.cpp)
target_link_libraries(usdGeoUsd_tests PRIVATE usdgeo::usd)
add_test(NAME usdGeoUsd_unit COMMAND usdGeoUsd_tests)