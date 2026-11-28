#include <gtest/gtest.h>

#include "esphome/components/duco/duco.h"

namespace esphome::duco::testing {

struct NetworkTypeCase {
  uint8_t type;
  const char *expected;
};

class NetworkTypeNameTest : public ::testing::TestWithParam<NetworkTypeCase> {};

TEST_P(NetworkTypeNameTest, Name) {
  const NetworkTypeCase &c = GetParam();
  EXPECT_STREQ(LOG_STR_ARG(network_type_name(c.type)), c.expected);
}

INSTANTIATE_TEST_SUITE_P(Duco, NetworkTypeNameTest,
                         ::testing::Values(NetworkTypeCase{0, "-"}, NetworkTypeCase{1, "Wired"},
                                           NetworkTypeCase{2, "RF"}, NetworkTypeCase{3, "Virtual"},
                                           NetworkTypeCase{4, "unknown"}, NetworkTypeCase{255, "unknown"}));

}  // namespace esphome::duco::testing
