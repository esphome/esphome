#include <gtest/gtest.h>

#include <cstring>

#include "esphome/components/socket/ipv4_resolve.h"

#ifdef USE_HOST

namespace esphome::socket::testing {

TEST(Ipv4Resolve, LiteralIsReadyWithoutDns) {
  Ipv4Resolve lookup;
  lookup.start("192.168.1.1", 1, "test");
  EXPECT_TRUE(lookup.ready());

  struct sockaddr_storage addr{};
  socklen_t len = lookup.to_sockaddr(reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr), 6053);
  ASSERT_EQ(len, sizeof(sockaddr_in));
  auto *in = reinterpret_cast<sockaddr_in *>(&addr);
  EXPECT_EQ(in->sin_family, AF_INET);
  EXPECT_EQ(ntohs(in->sin_port), 6053);
  EXPECT_EQ(in->sin_addr.s_addr, htonl(0xC0A80101));
}

TEST(Ipv4Resolve, ForgetDropsTheLiteral) {
  Ipv4Resolve lookup;
  lookup.start("10.0.0.5", 80, "test");
  ASSERT_TRUE(lookup.ready());
  lookup.forget();
  EXPECT_FALSE(lookup.ready());

  struct sockaddr_storage addr{};
  EXPECT_EQ(lookup.to_sockaddr(reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr), 80), 0u);
}

TEST(Ipv4Resolve, Ipv6LiteralIsRejected) {
  Ipv4Resolve lookup;
  lookup.start("::1", 443, "test");
  EXPECT_FALSE(lookup.ready());
  EXPECT_TRUE(lookup.consume_failure());
}

TEST(Ipv4Resolve, ShortBufferWritesNothing) {
  Ipv4Resolve lookup;
  lookup.start("192.0.2.10", 502, "test");
  ASSERT_TRUE(lookup.ready());
  struct sockaddr_in addr{};
  EXPECT_EQ(lookup.to_sockaddr(reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr) - 1, 502), 0u);
}

}  // namespace esphome::socket::testing

#endif
