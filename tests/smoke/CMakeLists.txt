# Build-skeleton smoke suite: proves configure, compile and CTest registration work.
add_executable(tgvc_smoke_tests smoke_test.cpp)
target_link_libraries(tgvc_smoke_tests PRIVATE
  tgvc_warnings
  tgvc_sanitizers
  GTest::gtest_main
)
gtest_discover_tests(tgvc_smoke_tests TEST_PREFIX "smoke." DISCOVERY_TIMEOUT 60)
