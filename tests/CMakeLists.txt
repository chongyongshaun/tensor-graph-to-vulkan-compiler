# Test suites and the repository test entry-point policy.
#
# CTest is the single entry point for every test in this repository:
#   * C++ policy, unit and IR tests are gtest suites registered here;
#   * Python tests are registered in this same suite with add_test() where the
#     backlog mandates Python (NumPy differential oracles, TF-025), so `ctest`
#     remains the one command that runs everything.
#
# Each subdirectory contributes one tgvc_<suite>_tests target.

include(GoogleTest)

add_subdirectory(smoke)
add_subdirectory(scope)
