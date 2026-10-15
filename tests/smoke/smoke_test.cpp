// Smoke test for the build skeleton: no product behaviour, only proof that the
// toolchain, gtest link and CTest registration are wired correctly.
#include <gtest/gtest.h>

static_assert(__cplusplus >= 202002L, "TGVC targets C++20 or newer");

TEST(BuildSkeleton, CompilesAsCxx20OrNewer) {
  EXPECT_GE(__cplusplus, 202002L);
}
