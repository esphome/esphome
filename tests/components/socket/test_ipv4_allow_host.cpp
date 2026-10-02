#include <gtest/gtest.h>

#include "esphome/components/socket/ipv4_allow.h"

#ifdef USE_HOST

namespace esphome::socket::testing {

TEST(Ipv4Allow, EmptyAllowsEveryAddress) {
  Ipv4Allow list;
  EXPECT_TRUE(list.empty());
  EXPECT_TRUE(list.allows(0));
  EXPECT_TRUE(list.allows(0xC0A8AF01));
}

TEST(Ipv4Allow, HostIsASingleAddress) {
  Ipv4Allow list;
  ASSERT_TRUE(list.add(0xC0A8AF14, 0xFFFFFFFF));
  EXPECT_TRUE(list.allows(0xC0A8AF14));
  EXPECT_FALSE(list.allows(0xC0A8AF15));
}

TEST(Ipv4Allow, NetworkClearsHostBits) {
  Ipv4Allow list;
  ASSERT_TRUE(list.add(0xC0A8AF21, 0xFFFFFF00));
  EXPECT_EQ(list.addr_at(0), 0xC0A8AF00);
  EXPECT_TRUE(list.allows(0xC0A8AF01));
  EXPECT_TRUE(list.allows(0xC0A8AFFF));
  EXPECT_FALSE(list.allows(0xC0A8B001));
}

TEST(Ipv4Allow, FullListRejectsAnother) {
  Ipv4Allow list;
  for (uint8_t i = 0; i < Ipv4Allow::MAX; i++) {
    ASSERT_TRUE(list.add(i, 0xFFFFFFFF));
  }
  EXPECT_FALSE(list.add(0x10, 0xFFFFFFFF));
  EXPECT_EQ(list.size(), Ipv4Allow::MAX);
  EXPECT_TRUE(list.allows(0));
  EXPECT_FALSE(list.allows(0x10));
}

}  // namespace esphome::socket::testing

#endif
