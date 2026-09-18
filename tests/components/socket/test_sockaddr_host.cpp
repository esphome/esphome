#include <gtest/gtest.h>

#include "esphome/components/socket/socket.h"

#ifdef USE_HOST

namespace esphome::socket::testing {

// =========================================================================
// set_sockaddr — IPv4
// =========================================================================

TEST(SetSockaddr, IPv4Basic) {
  struct sockaddr_storage addr {};
  socklen_t len = set_sockaddr((struct sockaddr *) &addr, sizeof(addr), "192.168.1.1", 6053);
  ASSERT_EQ(len, sizeof(sockaddr_in));
  auto *in = reinterpret_cast<sockaddr_in *>(&addr);
  EXPECT_EQ(in->sin_family, AF_INET);
  EXPECT_EQ(ntohs(in->sin_port), 6053);
}

TEST(SetSockaddr, IPv4Broadcast) {
  // inet_addr() returns INADDR_NONE for this literal too -- must not be treated as a parse
  // failure.
  struct sockaddr_storage addr {};
  socklen_t len = set_sockaddr((struct sockaddr *) &addr, sizeof(addr), "255.255.255.255", 12345);
  ASSERT_EQ(len, sizeof(sockaddr_in));
  auto *in = reinterpret_cast<sockaddr_in *>(&addr);
  // 255.255.255.255 happens to equal INADDR_NONE's bit pattern -- confirms the "valid
  // broadcast, not a parse failure" special case this function documents.
  EXPECT_EQ(in->sin_addr.s_addr, ESPHOME_INADDR_NONE);
}

TEST(SetSockaddr, IPv4MalformedYieldsZeroAndEINVAL) {
  struct sockaddr_storage addr {};
  errno = 0;
  socklen_t len = set_sockaddr((struct sockaddr *) &addr, sizeof(addr), "not-an-ip", 1);
  EXPECT_EQ(len, 0u);
  EXPECT_EQ(errno, EINVAL);
}

TEST(SetSockaddr, BufferTooSmallYieldsZero) {
  struct sockaddr_storage addr {};
  errno = 0;
  socklen_t len = set_sockaddr((struct sockaddr *) &addr, sizeof(sockaddr_in) - 1, "192.168.1.1", 1);
  EXPECT_EQ(len, 0u);
  EXPECT_EQ(errno, EINVAL);
}

TEST(SetSockaddr, StringOverloadMatchesCStringOverload) {
  struct sockaddr_storage addr_c {};
  struct sockaddr_storage addr_s {};
  socklen_t len_c = set_sockaddr((struct sockaddr *) &addr_c, sizeof(addr_c), "10.0.0.5", 80);
  socklen_t len_s = set_sockaddr((struct sockaddr *) &addr_s, sizeof(addr_s), std::string("10.0.0.5"), 80);
  ASSERT_EQ(len_c, len_s);
  EXPECT_EQ(std::memcmp(&addr_c, &addr_s, len_c), 0);
}

// =========================================================================
// set_sockaddr — IPv6
// =========================================================================

#if USE_NETWORK_IPV6

TEST(SetSockaddr, IPv6Basic) {
  struct sockaddr_storage addr {};
  socklen_t len = set_sockaddr((struct sockaddr *) &addr, sizeof(addr), "::1", 443);
  ASSERT_EQ(len, sizeof(sockaddr_in6));
  auto *in6 = reinterpret_cast<sockaddr_in6 *>(&addr);
  EXPECT_EQ(in6->sin6_family, AF_INET6);
  EXPECT_EQ(ntohs(in6->sin6_port), 443);
}

TEST(SetSockaddr, IPv6MalformedYieldsZeroAndEINVAL) {
  struct sockaddr_storage addr {};
  errno = 0;
  socklen_t len = set_sockaddr((struct sockaddr *) &addr, sizeof(addr), "gg::not-ipv6", 1);
  EXPECT_EQ(len, 0u);
  EXPECT_EQ(errno, EINVAL);
}

#endif  // USE_NETWORK_IPV6

// =========================================================================
// set_sockaddr_any
// =========================================================================

TEST(SetSockaddrAny, PortIsSetFamilyMatchesBuild) {
  struct sockaddr_storage addr {};
  socklen_t len = set_sockaddr_any((struct sockaddr *) &addr, sizeof(addr), 9999);
  ASSERT_GT(len, 0u);
#if USE_NETWORK_IPV6
  ASSERT_EQ(len, sizeof(sockaddr_in6));
  EXPECT_EQ(reinterpret_cast<sockaddr_in6 *>(&addr)->sin6_family, AF_INET6);
  EXPECT_EQ(ntohs(reinterpret_cast<sockaddr_in6 *>(&addr)->sin6_port), 9999);
#else
  ASSERT_EQ(len, sizeof(sockaddr_in));
  EXPECT_EQ(reinterpret_cast<sockaddr_in *>(&addr)->sin_family, AF_INET);
  EXPECT_EQ(ntohs(reinterpret_cast<sockaddr_in *>(&addr)->sin_port), 9999);
#endif
}

// =========================================================================
// format_sockaddr_to
// =========================================================================

TEST(FormatSockaddrTo, IPv4RoundTrip) {
  struct sockaddr_storage addr {};
  socklen_t len = set_sockaddr((struct sockaddr *) &addr, sizeof(addr), "203.0.113.7", 1);
  ASSERT_GT(len, 0u);
  char buf[SOCKADDR_STR_LEN];
  size_t written = format_sockaddr_to((struct sockaddr *) &addr, len, buf);
  EXPECT_GT(written, 0u);
  EXPECT_STREQ(buf, "203.0.113.7");
}

TEST(FormatSockaddrTo, UnknownFamilyYieldsEmptyString) {
  struct sockaddr addr {};
  addr.sa_family = AF_UNSPEC;
  char buf[SOCKADDR_STR_LEN] = {'x', '\0'};
  size_t written = format_sockaddr_to(&addr, sizeof(addr), buf);
  EXPECT_EQ(written, 0u);
  EXPECT_STREQ(buf, "");
}

#if USE_NETWORK_IPV6

TEST(FormatSockaddrTo, IPv6RoundTrip) {
  struct sockaddr_storage addr {};
  socklen_t len = set_sockaddr((struct sockaddr *) &addr, sizeof(addr), "2001:db8::1", 1);
  ASSERT_GT(len, 0u);
  char buf[SOCKADDR_STR_LEN];
  size_t written = format_sockaddr_to((struct sockaddr *) &addr, len, buf);
  EXPECT_GT(written, 0u);
  EXPECT_STREQ(buf, "2001:db8::1");
}

TEST(FormatSockaddrTo, V4MappedFormatsAsPlainIPv4) {
  struct sockaddr_storage addr {};
  socklen_t len = set_sockaddr((struct sockaddr *) &addr, sizeof(addr), "::ffff:198.51.100.9", 1);
  ASSERT_GT(len, 0u);
  char buf[SOCKADDR_STR_LEN];
  size_t written = format_sockaddr_to((struct sockaddr *) &addr, len, buf);
  EXPECT_GT(written, 0u);
  EXPECT_STREQ(buf, "198.51.100.9");
}

#endif  // USE_NETWORK_IPV6

// =========================================================================
// socket_ip / socket_ip_loop_monitored — domain selection
// =========================================================================

TEST(SocketIp, PicksDomainMatchingBuild) {
  auto sock = socket_ip(SOCK_DGRAM, IPPROTO_UDP);
  ASSERT_NE(sock, nullptr);
  struct sockaddr_storage addr {};
  socklen_t len = sizeof(addr);
  ASSERT_EQ(sock->getsockname((struct sockaddr *) &addr, &len), 0);
#if USE_NETWORK_IPV6
  EXPECT_EQ(addr.ss_family, AF_INET6);
#else
  EXPECT_EQ(addr.ss_family, AF_INET);
#endif
}

}  // namespace esphome::socket::testing

#endif  // USE_HOST
