#include <gtest/gtest.h>

#include <iterator>

#include "esphome/components/socket/ipv4_allow.h"
#include "esphome/components/socket/socket.h"

#ifdef USE_HOST

namespace esphome::socket::testing {

// The size_t count packs into the pointer's padding; no RAM over a uint8_t.
static_assert(sizeof(Ipv4Allow) == 2 * sizeof(void *), "unexpected padding in Ipv4Allow");

// 192.168.175.20/32 and 192.168.175.0/24, network order, host bits cleared,
// mirroring what add_ipv4_allow emits.
static const Ipv4AllowEntry ENTRIES[] = {
    {htonl(0xC0A8AF14), htonl(0xFFFFFFFF)},
    {htonl(0xC0A8AF00), htonl(0xFFFFFF00)},
};

// Runs the peer through the same parser production addresses go through.
static bool allows_peer(const Ipv4Allow &list, const char *ip) {
  struct sockaddr_storage peer {};
  EXPECT_NE(set_sockaddr(reinterpret_cast<struct sockaddr *>(&peer), sizeof(peer), ip, 0), 0);
  return list.allows(reinterpret_cast<const struct sockaddr *>(&peer));
}

TEST(Ipv4Allow, EmptyAllowsEveryPeer) {
  Ipv4Allow list;
  EXPECT_TRUE(list.allows(htonl(0xC0A8AF01)));
  EXPECT_TRUE(allows_peer(list, "10.0.0.1"));
  EXPECT_TRUE(allows_peer(list, "fe80::1"));
}

TEST(Ipv4Allow, MatchesHostAndNetworkEntries) {
  Ipv4Allow list;
  list.set(ENTRIES, std::size(ENTRIES));
  EXPECT_TRUE(list.allows(htonl(0xC0A8AF14)));
  EXPECT_TRUE(list.allows(htonl(0xC0A8AF01)));
  EXPECT_TRUE(list.allows(htonl(0xC0A8AFFF)));
  EXPECT_FALSE(list.allows(htonl(0xC0A8B001)));
}

TEST(Ipv4Allow, ChecksTheV4PeerInsideASockaddr) {
  Ipv4Allow list;
  list.set(ENTRIES, std::size(ENTRIES));
  EXPECT_TRUE(allows_peer(list, "192.168.175.66"));
  EXPECT_FALSE(allows_peer(list, "10.0.0.1"));
}

TEST(Ipv4Allow, UnwrapsAV4MappedIpv6Peer) {
  Ipv4Allow list;
  list.set(ENTRIES, std::size(ENTRIES));
  EXPECT_TRUE(allows_peer(list, "::ffff:192.168.175.66"));
  // A native IPv6 peer cannot match an IPv4 list.
  EXPECT_FALSE(allows_peer(list, "fe80::1"));
}

TEST(Ipv4Allow, InstancesKeepIndependentLists) {
  // One bridge per allow list; each instance points at its own entries.
  static const Ipv4AllowEntry OTHER[] = {{htonl(0x0A000000), htonl(0xFF000000)}};
  Ipv4Allow first;
  Ipv4Allow second;
  first.set(ENTRIES, std::size(ENTRIES));
  second.set(OTHER, std::size(OTHER));
  EXPECT_TRUE(first.allows(htonl(0xC0A8AF14)));
  EXPECT_FALSE(second.allows(htonl(0xC0A8AF14)));
  EXPECT_TRUE(second.allows(htonl(0x0A00002A)));
  EXPECT_FALSE(first.allows(htonl(0x0A00002A)));
}

TEST(Ipv4Allow, HostEntryMatchesOnlyThatAddress) {
  static const Ipv4AllowEntry HOST[] = {{htonl(0xC0A8AF14), htonl(0xFFFFFFFF)}};
  Ipv4Allow list;
  list.set(HOST, std::size(HOST));
  EXPECT_TRUE(allows_peer(list, "192.168.175.20"));
  EXPECT_FALSE(allows_peer(list, "192.168.175.21"));
  EXPECT_FALSE(allows_peer(list, "192.168.175.19"));
}

TEST(Ipv4Allow, CatchAllAllowsEveryV4PeerOnly) {
  static const Ipv4AllowEntry ANY[] = {{0, 0}};
  Ipv4Allow list;
  list.set(ANY, std::size(ANY));
  EXPECT_TRUE(allows_peer(list, "0.0.0.0"));
  EXPECT_TRUE(allows_peer(list, "255.255.255.255"));
  EXPECT_TRUE(allows_peer(list, "::ffff:10.1.2.3"));
  // Unlike an empty list, 0.0.0.0/0 still turns a native IPv6 peer away.
  EXPECT_FALSE(allows_peer(list, "fe80::1"));
}

TEST(Ipv4Allow, LastEntryOfAFullListMatches) {
  // 255 is the schema's cap: 10.0.0.1/32 to 10.0.0.255/32.
  static Ipv4AllowEntry full[255];
  for (uint32_t i = 0; i < std::size(full); i++) {
    full[i] = {htonl(0x0A000001 + i), htonl(0xFFFFFFFF)};
  }
  Ipv4Allow list;
  list.set(full, std::size(full));
  EXPECT_EQ(list.size(), 255u);
  EXPECT_TRUE(allows_peer(list, "10.0.0.1"));
  EXPECT_TRUE(allows_peer(list, "10.0.0.255"));
  EXPECT_FALSE(allows_peer(list, "10.0.1.0"));
  EXPECT_FALSE(allows_peer(list, "10.0.0.0"));
}

}  // namespace esphome::socket::testing

#endif
