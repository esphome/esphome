#pragma once
#include "esphome/components/packet_interface/packet_interface.h"
#include "esphome/core/log.h"

namespace pisim {
using esphome::packet_interface::PacketBuffer;
using esphome::packet_interface::PacketInterface;
using esphome::packet_interface::PacketMetaData;
struct Side {
  const char *name;
  PacketInterface *top{nullptr};
  uint32_t rx_ok{0}, rx_bad{0}, rx_bytes{0}, tx_ok{0}, tx_refused{0};
  uint16_t sent_len[256]{};  // length sent for index i, at i & 0xFF; the peer checks received sizes against it
  bool up{false};
};
inline Side a{"A"}, b{"B"};
inline Side &peer(const Side &s) { return &s == &a ? b : a; }
// payload i: length n, byte k = pat(i, k), first two bytes carry i
inline uint8_t pat(uint16_t i, size_t k) { return (uint8_t) ((i * 31 + k) & 0xFF); }
inline void fill(uint8_t *buf, size_t n, uint16_t i) {
  for (size_t k = 0; k < n; k++)
    buf[k] = pat(i, k);
  if (n >= 2) {
    buf[0] = i >> 8;
    buf[1] = i & 0xFF;
  }
}
inline void attach(Side &s, PacketInterface *link) {
  s.top = link->top();
  s.top->add_on_session_state_callback([&s](bool c) {
    s.up = c;
    ESP_LOGI("pisim", "PI %s session %s", s.name, c ? "up" : "down");
  });
  s.top->add_packet_interface_listener([&s](const PacketBuffer &p, const PacketMetaData &) {
    size_t n = p.size();
    if (n < 2) {
      s.rx_bad++;
      return;
    }
    uint16_t i = (p[0] << 8) | p[1];
    bool ok = n == peer(s).sent_len[i & 0xFF];  // a truncated or split packet fails here
    for (size_t k = 2; k < n; k++)
      ok &= p[k] == pat(i, k);
    ok ? s.rx_ok++ : s.rx_bad++;
    s.rx_bytes += n;
  });
  ESP_LOGI("pisim", "PI %s top max_packet_size=%u", s.name, (unsigned) s.top->get_max_packet_size());
}
inline bool send(Side &s, uint16_t i, size_t n) {
  static uint8_t buf[2048];
  if (n > sizeof(buf)) {
    s.tx_refused++;
    return false;
  }
  fill(buf, n, i);
  s.sent_len[i & 0xFF] = n;
  PacketBuffer p(buf, n);
  bool ok = s.top->send_to_interface(p, PacketMetaData{});
  ok ? s.tx_ok++ : s.tx_refused++;
  return ok;
}
inline void report(const char *tag) {
  for (Side *s : {&a, &b})
    ESP_LOGI("pisim", "PI %s %s up=%d tx_ok=%u refused=%u rx_ok=%u rx_bad=%u rx_bytes=%u", tag, s->name, s->up,
             (unsigned) s->tx_ok, (unsigned) s->tx_refused, (unsigned) s->rx_ok, (unsigned) s->rx_bad,
             (unsigned) s->rx_bytes);
}
}  // namespace pisim
