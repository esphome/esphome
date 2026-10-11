#include "ipv4_resolve_test_component.h"
#include "esphome/components/socket/ipv4_resolve.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome::ipv4_resolve_test_component {

static const char *const TAG = "ipv4_resolve_test";

static bool check_sockaddr(socket::Ipv4Resolve &lookup, uint16_t port, uint32_t expected) {
  struct sockaddr_storage addr {};
  socklen_t len = lookup.to_sockaddr(reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr), port);
  if (len != sizeof(sockaddr_in)) {
    return false;
  }
  auto *in = reinterpret_cast<sockaddr_in *>(&addr);
  return in->sin_family == AF_INET && ntohs(in->sin_port) == port && in->sin_addr.s_addr == htonl(expected);
}

void Ipv4ResolveTestComponent::setup() {
  ESP_LOGI(TAG, "IPv4 resolve test starting");

  socket::Ipv4Resolve lookup;
  struct sockaddr_storage addr {};

  lookup.start("192.168.1.1", 1, TAG);
  bool ok = lookup.ready() && check_sockaddr(lookup, 6053, 0xC0A80101);
  ESP_LOGI(TAG, "Literal resolve: %s", ok ? LOG_STR_LITERAL("PASSED") : LOG_STR_LITERAL("FAILED"));

  lookup.forget();
  ok = !lookup.ready() && lookup.to_sockaddr(reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr), 80) == 0;
  ESP_LOGI(TAG, "Forget drops address: %s", ok ? LOG_STR_LITERAL("PASSED") : LOG_STR_LITERAL("FAILED"));

  lookup.start("::1", 443, TAG);
  ok = !lookup.ready() && lookup.consume_failure();
  ESP_LOGI(TAG, "IPv6 literal rejected: %s", ok ? LOG_STR_LITERAL("PASSED") : LOG_STR_LITERAL("FAILED"));

  lookup.start("localhost", 6053, TAG);
  ok = lookup.ready() && check_sockaddr(lookup, 6053, 0x7F000001);
  ESP_LOGI(TAG, "Hostname resolve: %s", ok ? LOG_STR_LITERAL("PASSED") : LOG_STR_LITERAL("FAILED"));

  ESP_LOGI(TAG, "IPv4 resolve test complete");
}

}  // namespace esphome::ipv4_resolve_test_component
