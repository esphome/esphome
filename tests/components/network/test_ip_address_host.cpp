#include <gtest/gtest.h>

#include "esphome/components/network/ip_address.h"

#ifdef USE_HOST
#if USE_NETWORK_IPV6

namespace esphome::network::testing {

// =========================================================================
// IPv4
// =========================================================================

TEST(IPAddressHost, IPv4DefaultNotSet) {
  IPAddress addr;
  EXPECT_FALSE(addr.is_set());
}

TEST(IPAddressHost, IPv4DefaultIsIPv4) {
  IPAddress addr;
  EXPECT_TRUE(addr.is_ip4());
  EXPECT_FALSE(addr.is_ip6());
}

TEST(IPAddressHost, IPv4ParseAndSerialize) {
  IPAddress addr("192.168.1.1");
  char buf[IP_ADDRESS_BUFFER_SIZE];
  EXPECT_STREQ(addr.str_to(buf), "192.168.1.1");
}

TEST(IPAddressHost, IPv4FromOctets) {
  IPAddress addr(192, 168, 1, 1);
  char buf[IP_ADDRESS_BUFFER_SIZE];
  EXPECT_STREQ(addr.str_to(buf), "192.168.1.1");
}

TEST(IPAddressHost, IPv4IsSet) {
  IPAddress addr("192.168.1.1");
  EXPECT_TRUE(addr.is_set());
}

TEST(IPAddressHost, IPv4IsIp4) {
  IPAddress addr("192.168.1.1");
  EXPECT_TRUE(addr.is_ip4());
  EXPECT_FALSE(addr.is_ip6());
}

TEST(IPAddressHost, IPv4MulticastDetected) {
  IPAddress addr("239.0.60.53");
  EXPECT_TRUE(addr.is_multicast());
}

TEST(IPAddressHost, IPv4MulticastBoundaryLow) {
  IPAddress addr("224.0.0.0");
  EXPECT_TRUE(addr.is_multicast());
}

TEST(IPAddressHost, IPv4MulticastBoundaryHigh) {
  IPAddress addr("239.255.255.255");
  EXPECT_TRUE(addr.is_multicast());
}

TEST(IPAddressHost, IPv4UnicastNotMulticast) {
  IPAddress addr("192.168.1.1");
  EXPECT_FALSE(addr.is_multicast());
}

TEST(IPAddressHost, IPv4EqualityMatch) {
  IPAddress a("192.168.1.1");
  IPAddress b("192.168.1.1");
  EXPECT_EQ(a, b);
}

TEST(IPAddressHost, IPv4EqualityMismatch) {
  IPAddress a("192.168.1.1");
  IPAddress b("192.168.1.2");
  EXPECT_NE(a, b);
}

TEST(IPAddressHost, IPv4FromOctetsMatchesParse) {
  IPAddress from_octets(192, 168, 1, 1);
  IPAddress from_string("192.168.1.1");
  EXPECT_EQ(from_octets, from_string);
}

TEST(IPAddressHost, IPv4FromIPAddrT) {
  ip_addr_t raw;
  memset(&raw, 0, sizeof(raw));
  raw.u_addr.ip4.s_addr = htonl((192u << 24) | (168u << 16) | (1u << 8) | 1u);
  raw.type = IPADDR_TYPE_V4;
  IPAddress addr(&raw);
  char buf[IP_ADDRESS_BUFFER_SIZE];
  EXPECT_STREQ(addr.str_to(buf), "192.168.1.1");
  EXPECT_TRUE(addr.is_ip4());
  EXPECT_FALSE(addr.is_ip6());
}

// =========================================================================
// IPv6
// =========================================================================

TEST(IPAddressHost, IPv6ParseAndSerialize) {
  IPAddress addr("ff12::cafe");
  char buf[IP_ADDRESS_BUFFER_SIZE];
  EXPECT_STREQ(addr.str_to(buf), "ff12::cafe");
}

TEST(IPAddressHost, IPv6Loopback) {
  IPAddress addr("::1");
  char buf[IP_ADDRESS_BUFFER_SIZE];
  EXPECT_STREQ(addr.str_to(buf), "::1");
}

TEST(IPAddressHost, IPv6IsIp6) {
  IPAddress addr("ff12::cafe");
  EXPECT_TRUE(addr.is_ip6());
  EXPECT_FALSE(addr.is_ip4());
}

TEST(IPAddressHost, IPv6AllZerosNotSet) {
  IPAddress addr("::");
  EXPECT_FALSE(addr.is_set());
}

TEST(IPAddressHost, IPv6LoopbackIsSet) {
  IPAddress addr("::1");
  EXPECT_TRUE(addr.is_set());
}

TEST(IPAddressHost, IPv6MulticastDetected) {
  IPAddress addr("ff12::cafe");
  EXPECT_TRUE(addr.is_multicast());
}

TEST(IPAddressHost, IPv6MulticastLinkLocal) {
  IPAddress addr("ff02::1");
  EXPECT_TRUE(addr.is_multicast());
}

TEST(IPAddressHost, IPv6UnicastNotMulticast) {
  IPAddress addr("::1");
  EXPECT_FALSE(addr.is_multicast());
}

TEST(IPAddressHost, IPv6EqualityMatch) {
  IPAddress a("ff12::cafe");
  IPAddress b("ff12::cafe");
  EXPECT_EQ(a, b);
}

TEST(IPAddressHost, IPv6EqualityMismatch) {
  IPAddress a("ff12::cafe");
  IPAddress b("ff02::1");
  EXPECT_NE(a, b);
}

TEST(IPAddressHost, IPv6OutputIsLowercase) {
  // inet_pton is case-insensitive; str_to must lowercase the output
  IPAddress addr("FF12::CAFE");
  char buf[IP_ADDRESS_BUFFER_SIZE];
  addr.str_to(buf);
  for (const char *p = buf; *p; ++p) {
    EXPECT_FALSE(*p >= 'A' && *p <= 'F') << "uppercase letter in: " << buf;
  }
}

TEST(IPAddressHost, IPv6FullAddressRoundTrip) {
  // A full 128-bit address with no compression opportunity
  const char *input = "fde0:983a:d0d3:a65e:725a:0fff:fe36:9916";
  IPAddress addr(input);
  char buf[IP_ADDRESS_BUFFER_SIZE];
  addr.str_to(buf);
  EXPECT_NE(buf[0], '\0');
  EXPECT_NE(std::string(buf).find("fde0"), std::string::npos);
}

// =========================================================================
// Malformed input
// =========================================================================

TEST(IPAddressHost, MalformedIPv4YieldsEmptyAddress) {
  IPAddress addr("not-an-ip");
  EXPECT_FALSE(addr.is_set());
  EXPECT_TRUE(addr.is_ip4());
}

TEST(IPAddressHost, MalformedIPv6YieldsEmptyAddress) {
  // "gg::1" looks like IPv6 (contains ':') but fails inet_pton; addr stays
  // zeroed (type=V4 from memset) so is_set() is false and is_ip4() is true.
  IPAddress addr("gg::1");
  EXPECT_FALSE(addr.is_set());
  EXPECT_TRUE(addr.is_ip4());
}

// =========================================================================
// Cross-family
// =========================================================================

TEST(IPAddressHost, IPv4AndIPv6NotEqual) {
  IPAddress v4("192.168.1.1");
  IPAddress v6("::1");
  EXPECT_NE(v4, v6);
}

}  // namespace esphome::network::testing

#endif  // USE_NETWORK_IPV6
#endif  // USE_HOST
