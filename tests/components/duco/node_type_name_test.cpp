#include <cstring>

#include <gtest/gtest.h>

#include "esphome/components/duco/duco.h"

namespace esphome::duco::testing {

struct NodeTypeCase {
  uint8_t type;
  const char *expected;
};

class NodeTypeNameTest : public ::testing::TestWithParam<NodeTypeCase> {};

TEST_P(NodeTypeNameTest, Name) {
  const NodeTypeCase &c = GetParam();
  EXPECT_STREQ(LOG_STR_ARG(node_type_name(c.type, 0)), c.expected);
}

// The table ends, out of range types, and the box sensor variants
INSTANTIATE_TEST_SUITE_P(Duco, NodeTypeNameTest,
                         ::testing::Values(NodeTypeCase{1, "IQ"}, NodeTypeCase{10, "UCRH"}, NodeTypeCase{49, "CSCO2"},
                                           NodeTypeCase{0, "UNKNOWN"}, NodeTypeCase{50, "UNKNOWN"},
                                           NodeTypeCase{0x7F, "UNKNOWN"}));

TEST(NodeTypeNetworkTest, BoxSensorVariants) {
  for (uint8_t network = 0; network < 3; network++) {
    EXPECT_STREQ(LOG_STR_ARG(node_type_name(10, network)), "UCRH");
    EXPECT_STREQ(LOG_STR_ARG(node_type_name(12, network)), "UCCO2");
  }
  EXPECT_STREQ(LOG_STR_ARG(node_type_name(10, 3)), "BSRH");
  EXPECT_STREQ(LOG_STR_ARG(node_type_name(12, 3)), "BSCO2");
  EXPECT_STREQ(LOG_STR_ARG(node_type_name(17, 3)), "BOX");
  EXPECT_STREQ(LOG_STR_ARG(node_type_name(255, 3)), "UNKNOWN");
}

}  // namespace esphome::duco::testing
