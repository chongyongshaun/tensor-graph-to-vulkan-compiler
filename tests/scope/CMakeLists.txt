# Scope-matrix suite.
#
# The TF-002 deliverable adds scope_matrix_test.cpp here. Until that file exists the
# directory is allowed to be empty, so the build skeleton stands on its own.
file(GLOB scope_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
if(NOT scope_sources)
  return()
endif()

add_executable(tgvc_scope_tests ${scope_sources})
target_link_libraries(tgvc_scope_tests PRIVATE
  tgvc_warnings
  tgvc_sanitizers
  GTest::gtest_main
  nlohmann_json::nlohmann_json
)
# The suite reads docs/SCOPE.md and schemas/tgvc-operators-v1.json from the source tree.
target_compile_definitions(tgvc_scope_tests PRIVATE
  TGVC_SOURCE_ROOT="${PROJECT_SOURCE_DIR}"
)
gtest_discover_tests(tgvc_scope_tests TEST_PREFIX "scope." DISCOVERY_TIMEOUT 60)
