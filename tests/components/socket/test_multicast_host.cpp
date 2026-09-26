#include <gtest/gtest.h>

#include "esphome/components/socket/socket.h"

#ifdef USE_HOST

namespace esphome::socket::testing {

// =========================================================================
// join_multicast_group — EINVAL on malformed input
// =========================================================================

TEST(JoinMulticastGroup, MalformedIPv4YieldsEINVAL) {
  auto sock = esphome::socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  ASSERT_NE(sock, nullptr);
  errno = 0;
  EXPECT_FALSE(join_multicast_group(sock.get(), "not-an-ip"));
  EXPECT_EQ(errno, EINVAL);
}

TEST(JoinMulticastGroup, MalformedIPv6YieldsEINVAL) {
  // ':' present but parse fails — EINVAL regardless of USE_NETWORK_IPV6
  auto sock = esphome::socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  ASSERT_NE(sock, nullptr);
  errno = 0;
  EXPECT_FALSE(join_multicast_group(sock.get(), "gg::not-ipv6"));
  EXPECT_EQ(errno, EINVAL);
}

// =========================================================================
// join_multicast_group — IPv4 IP_ADD_MEMBERSHIP
// =========================================================================

TEST(JoinMulticastGroup, IPv4JoinSetsIfIndexToZero) {
  auto sock = esphome::socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  ASSERT_NE(sock, nullptr);
  uint32_t if_index = 99;
  EXPECT_TRUE(join_multicast_group(sock.get(), "239.0.60.53", &if_index));
  EXPECT_EQ(if_index, 0u);
}

TEST(JoinMulticastGroup, IPv4JoinNullIfIndexOut) {
  auto sock = esphome::socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  ASSERT_NE(sock, nullptr);
  EXPECT_TRUE(join_multicast_group(sock.get(), "239.0.60.53", nullptr));
}

// =========================================================================
// join_multicast_group — IPv6 and set_ipv6_multicast_if
// =========================================================================

#if USE_NETWORK_IPV6

TEST(JoinMulticastGroup, IPv6JoinSucceedsOrNoInterface) {
  auto sock = esphome::socket::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  ASSERT_NE(sock, nullptr);
  uint32_t if_index = 0;
  bool ok = join_multicast_group(sock.get(), "ff12::cafe", &if_index);
  if (ok) {
    EXPECT_GT(if_index, 0u);
  } else {
    EXPECT_EQ(errno, EADDRNOTAVAIL);
  }
}

TEST(SetIPv6MulticastIf, ProbeWithZeroIndexDoesNotCrash) {
  auto sock = esphome::socket::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  ASSERT_NE(sock, nullptr);
  // Probes for first eligible non-loopback interface — succeeds or fails gracefully
  (void) set_ipv6_multicast_if(sock.get(), 0);
}

TEST(SetIPv6MulticastIf, ExplicitIndexFromJoinSucceeds) {
  auto sock_join = esphome::socket::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  auto sock_send = esphome::socket::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  ASSERT_NE(sock_join, nullptr);
  ASSERT_NE(sock_send, nullptr);
  uint32_t if_index = 0;
  bool joined = join_multicast_group(sock_join.get(), "ff12::cafe", &if_index);
  if (joined && if_index != 0) {
    EXPECT_TRUE(set_ipv6_multicast_if(sock_send.get(), if_index));
  }
}

#endif  // USE_NETWORK_IPV6

}  // namespace esphome::socket::testing

#endif  // USE_HOST
