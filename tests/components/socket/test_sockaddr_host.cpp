#include <gtest/gtest.h>

#include <cerrno>
#include <cstring>

#include "esphome/components/socket/socket.h"

#ifdef USE_HOST

namespace esphome::socket::testing {

// =========================================================================
// Independent of USE_NETWORK_IPV6
// =========================================================================

TEST(SetSockaddr, IPv4Basic) {
  struct sockaddr_storage addr {};
  socklen_t len = set_sockaddr((struct sockaddr *) &addr, sizeof(addr), "192.168.1.1", 6053);
  ASSERT_EQ(len, sizeof(sockaddr_in));
  auto *in = reinterpret_cast<sockaddr_in *>(&addr);
  EXPECT_EQ(in->sin_family, AF_INET);
  EXPECT_EQ(ntohs(in->sin_port), 6053);
  EXPECT_EQ(in->sin_addr.s_addr, htonl(0xC0A80101));
}

TEST(SetSockaddr, IPv4Broadcast) {
  // 255.255.255.255 is INADDR_NONE's bit pattern but also a valid address.
  struct sockaddr_storage addr {};
  socklen_t len = set_sockaddr((struct sockaddr *) &addr, sizeof(addr), "255.255.255.255", 12345);
  ASSERT_EQ(len, sizeof(sockaddr_in));
  auto *in = reinterpret_cast<sockaddr_in *>(&addr);
  EXPECT_EQ(in->sin_addr.s_addr, ESPHOME_INADDR_NONE);
}

TEST(SetSockaddr, IPv4MalformedYieldsZeroAndEINVAL) {
  struct sockaddr_storage addr {};
  errno = 0;
  socklen_t len = set_sockaddr((struct sockaddr *) &addr, sizeof(addr), "not-an-ip", 1);
  EXPECT_EQ(len, 0u);
  EXPECT_EQ(errno, EINVAL);
}

TEST(SetSockaddr, IPv4BufferTooSmallYieldsZeroAndEINVAL) {
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

TEST(FormatSockaddrTo, IPv4RoundTrip) {
  struct sockaddr_storage addr {};
  socklen_t len = set_sockaddr((struct sockaddr *) &addr, sizeof(addr), "203.0.113.7", 1);
  ASSERT_GT(len, 0u);
  char buf[SOCKADDR_STR_LEN];
  size_t written = format_sockaddr_to((struct sockaddr *) &addr, len, buf);
  EXPECT_GT(written, 0u);
  EXPECT_STREQ(buf, "203.0.113.7");
}

TEST(FormatSockaddrTo, UnknownFamilyYieldsFamilyMarker) {
  struct sockaddr addr {};
  addr.sa_family = AF_UNSPEC;
  char buf[SOCKADDR_STR_LEN] = {'x', '\0'};
  size_t written = format_sockaddr_to(&addr, sizeof(addr), buf);
  EXPECT_EQ(written, strlen("<af=0>"));
  EXPECT_STREQ(buf, "<af=0>");
}

// =========================================================================
// IPv6 - this directory's __init__.py forces USE_NETWORK_IPV6 on
// =========================================================================

#if USE_NETWORK_IPV6

TEST(SetSockaddr, IPv6Basic) {
  struct sockaddr_storage addr {};
  socklen_t len = set_sockaddr((struct sockaddr *) &addr, sizeof(addr), "::1", 443);
  ASSERT_EQ(len, sizeof(sockaddr_in6));
  auto *in6 = reinterpret_cast<sockaddr_in6 *>(&addr);
  EXPECT_EQ(in6->sin6_family, AF_INET6);
  EXPECT_EQ(ntohs(in6->sin6_port), 443);
  EXPECT_TRUE(IN6_IS_ADDR_LOOPBACK(&in6->sin6_addr));
}

TEST(SetSockaddr, IPv6MalformedYieldsZeroAndEINVAL) {
  struct sockaddr_storage addr {};
  errno = 0;
  socklen_t len = set_sockaddr((struct sockaddr *) &addr, sizeof(addr), "gg::not-ipv6", 1);
  EXPECT_EQ(len, 0u);
  EXPECT_EQ(errno, EINVAL);
}

TEST(SetSockaddrAny, FillsIPv6AnyAddressWithPort) {
  struct sockaddr_storage addr {};
  socklen_t len = set_sockaddr_any((struct sockaddr *) &addr, sizeof(addr), 9999);
  ASSERT_EQ(len, sizeof(sockaddr_in6));
  auto *in6 = reinterpret_cast<sockaddr_in6 *>(&addr);
  EXPECT_EQ(in6->sin6_family, AF_INET6);
  EXPECT_EQ(ntohs(in6->sin6_port), 9999);
  EXPECT_TRUE(IN6_IS_ADDR_UNSPECIFIED(&in6->sin6_addr));
}

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

TEST(SocketIp, CreatesIPv6Socket) {
  auto sock = socket_ip(SOCK_DGRAM, IPPROTO_UDP);
  ASSERT_NE(sock, nullptr);
  struct sockaddr_storage addr {};
  socklen_t len = sizeof(addr);
  ASSERT_EQ(sock->getsockname((struct sockaddr *) &addr, &len), 0);
  EXPECT_EQ(addr.ss_family, AF_INET6);
}

#endif  // USE_NETWORK_IPV6

}  // namespace esphome::socket::testing

#endif  // USE_HOST
