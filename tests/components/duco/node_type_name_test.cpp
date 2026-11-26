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
  EXPECT_STREQ(LOG_STR_ARG(node_type_name(c.type)), c.expected);
}

// The table ends, out of range types, and the box sensor variants
INSTANTIATE_TEST_SUITE_P(Duco, NodeTypeNameTest,
                         ::testing::Values(NodeTypeCase{1, "IQ"}, NodeTypeCase{10, "UCRH"}, NodeTypeCase{49, "CSCO2"},
                                           NodeTypeCase{0, "UNKNOWN"}, NodeTypeCase{50, "UNKNOWN"},
                                           NodeTypeCase{0x7F, "UNKNOWN"}, NodeTypeCase{10 | NODE_TYPE_BS, "BSRH"},
                                           NodeTypeCase{12 | NODE_TYPE_BS, "BSCO2"},
                                           NodeTypeCase{17 | NODE_TYPE_BS, "BOX"}));

}  // namespace esphome::duco::testing
