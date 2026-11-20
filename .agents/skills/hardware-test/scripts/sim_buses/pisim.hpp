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
  uint32_t rx_ok{0}, rx_bad{0}, rx_lost{0}, rx_bytes{0}, tx_ok{0}, tx_refused{0};
  uint16_t rx_next{0};  // next index expected from the peer, which sends 0, 1, 2, ...
  // index and length sent, at i & 0xFF; the peer checks received sizes against it
  struct Sent {
    uint16_t idx, len;
  } sent[256]{};
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
    // a skipped index is a packet that never arrived; an old or repeated one counts as bad
    uint16_t gap = i - s.rx_next;
    if (gap >= 0x8000) {
      s.rx_bad++;
      return;
    }
    s.rx_lost += gap;
    s.rx_next = i + 1;
    // a truncated or split packet fails here, and so does one whose slot was reused by a later index
    const auto &sent = peer(s).sent[i & 0xFF];
    bool ok = sent.idx == i && n == sent.len;
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
  s.sent[i & 0xFF] = {i, (uint16_t) n};
  PacketBuffer p(buf, n);
  bool ok = s.top->send_to_interface(p, PacketMetaData{});
  ok ? s.tx_ok++ : s.tx_refused++;
  return ok;
}
// idle: both links have drained, so a packet sent but neither received nor counted lost is lost too, and the line
// ends in PASS or FAIL
inline void report(const char *tag, bool idle = false) {
  for (Side *s : {&a, &b}) {
    uint32_t sent = peer(*s).tx_ok;
    int32_t missing = (int32_t) (sent - s->rx_ok - s->rx_bad - s->rx_lost);
    bool clean = !peer(*s).tx_refused && !s->rx_bad && !s->rx_lost;
    const char *verdict = !idle ? "" : (sent && s->rx_ok == sent && clean ? " PASS" : " FAIL");
    ESP_LOGI("pisim", "PI %s %s up=%d tx_ok=%u refused=%u rx_ok=%u rx_bad=%u rx_lost=%u missing=%d rx_bytes=%u%s", tag,
             s->name, s->up, (unsigned) s->tx_ok, (unsigned) s->tx_refused, (unsigned) s->rx_ok, (unsigned) s->rx_bad,
             (unsigned) s->rx_lost, (int) missing, (unsigned) s->rx_bytes, verdict);
  }
}
}  // namespace pisim
