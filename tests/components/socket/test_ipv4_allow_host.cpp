#include <gtest/gtest.h>

#include <iterator>

#include "esphome/components/socket/ipv4_allow.h"
#include "esphome/components/socket/socket.h"

#ifdef USE_HOST

namespace esphome::socket::testing {

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

}  // namespace esphome::socket::testing

#endif
