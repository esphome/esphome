#include <gtest/gtest.h>

#include <cstring>

#include "esphome/components/socket/ipv4_allow.h"

#ifdef USE_HOST

namespace esphome::socket::testing {

// 192.168.175.20/32 and 192.168.175.0/24, network order, host bits cleared.
static const Ipv4AllowEntry ENTRIES[] = {
    {htonl(0xC0A8AF14), htonl(0xFFFFFFFF)},
    {htonl(0xC0A8AF00), htonl(0xFFFFFF00)},
};

static struct sockaddr_storage v4_peer(uint32_t addr_host_order) {
  struct sockaddr_storage storage {};
  auto *in = reinterpret_cast<struct sockaddr_in *>(&storage);
  in->sin_family = AF_INET;
  in->sin_addr.s_addr = htonl(addr_host_order);
  return storage;
}

static struct sockaddr_storage v6_peer(const uint8_t bytes[16]) {
  struct sockaddr_storage storage {};
  auto *in6 = reinterpret_cast<struct sockaddr_in6 *>(&storage);
  in6->sin6_family = AF_INET6;
  std::memcpy(in6->sin6_addr.s6_addr, bytes, 16);
  return storage;
}

TEST(Ipv4Allow, EmptyAllowsEveryPeer) {
  Ipv4Allow list;
  EXPECT_TRUE(list.allows(htonl(0xC0A8AF01)));
  auto peer = v4_peer(0x0A000001);
  EXPECT_TRUE(list.allows(reinterpret_cast<const struct sockaddr *>(&peer)));
}

TEST(Ipv4Allow, MatchesHostAndNetworkEntries) {
  Ipv4Allow list;
  list.set(ENTRIES, 2);
  EXPECT_TRUE(list.allows(htonl(0xC0A8AF14)));
  EXPECT_TRUE(list.allows(htonl(0xC0A8AF01)));
  EXPECT_TRUE(list.allows(htonl(0xC0A8AFFF)));
  EXPECT_FALSE(list.allows(htonl(0xC0A8B001)));
}

TEST(Ipv4Allow, ChecksTheV4PeerInsideASockaddr) {
  Ipv4Allow list;
  list.set(ENTRIES, 2);
  auto allowed = v4_peer(0xC0A8AF42);
  auto denied = v4_peer(0x0A000001);
  EXPECT_TRUE(list.allows(reinterpret_cast<const struct sockaddr *>(&allowed)));
  EXPECT_FALSE(list.allows(reinterpret_cast<const struct sockaddr *>(&denied)));
}

TEST(Ipv4Allow, UnwrapsAV4MappedIpv6Peer) {
  Ipv4Allow list;
  list.set(ENTRIES, 2);
  const uint8_t mapped[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0xC0, 0xA8, 0xAF, 0x42};
  const uint8_t native[16] = {0xFE, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
  auto mapped_peer = v6_peer(mapped);
  auto native_peer = v6_peer(native);
  EXPECT_TRUE(list.allows(reinterpret_cast<const struct sockaddr *>(&mapped_peer)));
  // A native IPv6 peer cannot match an IPv4 list.
  EXPECT_FALSE(list.allows(reinterpret_cast<const struct sockaddr *>(&native_peer)));
  Ipv4Allow empty;
  EXPECT_TRUE(empty.allows(reinterpret_cast<const struct sockaddr *>(&native_peer)));
}

}  // namespace esphome::socket::testing

#endif
