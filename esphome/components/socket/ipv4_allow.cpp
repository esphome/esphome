#include "ipv4_allow.h"

#ifdef USE_SOCKET_IPV4_ALLOW

#include "esphome/core/log.h"

#include <span>

namespace esphome::socket {

bool Ipv4Allow::admit(const struct sockaddr *peer, socklen_t len, const char *tag, uint32_t now) {
  if (this->allows(peer)) {
    return true;
  }
  if (this->last_reject_log_ms_ == 0 || now - this->last_reject_log_ms_ >= REJECT_LOG_INTERVAL_MS) {
    this->last_reject_log_ms_ = now;
    char text[SOCKADDR_STR_LEN];
    format_sockaddr_to(peer, len, std::span<char, SOCKADDR_STR_LEN>(text));
    ESP_LOGW(tag, "Rejected %s", text);
  }
  return false;
}

void Ipv4Allow::dump_config(const char *tag) const {
  for (size_t i = 0; i < this->count_; i++) {
    const Ipv4AllowEntry e = this->entry(i);
    // Network order is dotted order, and the contiguous mask's popcount is the prefix.
    const auto *b = reinterpret_cast<const uint8_t *>(&e.addr);
    ESP_LOGCONFIG(tag, "  Allowed IP: %u.%u.%u.%u/%u", b[0], b[1], b[2], b[3],
                  static_cast<unsigned>(__builtin_popcount(e.mask)));
  }
}

}  // namespace esphome::socket

#endif
