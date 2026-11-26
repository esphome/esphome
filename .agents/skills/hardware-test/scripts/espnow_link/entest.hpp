#pragma once
// Checked packet streams for the packet_interface link tests (esphome#21120).
// Packet: magic (2) | seq (2, LE) | sent size (2, LE) | bytes (seq + i). A reliable stream starts with 0x1E so
// entity_transport on the same stack drops it as an unknown packet type.
#include <cstdint>
#include <cstring>
#include "esphome/core/log.h"
#include "esphome/components/packet_interface/packet_interface.h"

namespace entest {
using esphome::packet_interface::PacketBuffer;
using esphome::packet_interface::PacketInterface;
using esphome::packet_interface::PacketMetaData;

struct Stream {
  const char *name;
  uint8_t m0, m1;
  uint32_t tx{0}, ref{0}, rx{0}, ok{0}, bad{0}, gap{0}, dup{0}, trunc{0}, foreign{0}, big{0}, maxsz{0};
  int32_t last{-1};
  uint16_t seq{0};
  uint32_t idx{0};

  Stream(const char *n, uint8_t a, uint8_t b) : name(n), m0(a), m1(b) {}

  void listen(PacketInterface *link) {
    link->add_packet_interface_listener([this](const PacketBuffer &d, const PacketMetaData &) { this->on_rx(d); });
  }
  void on_rx(const PacketBuffer &d) {
    static uint8_t buf[1600];
    size_t n = d.size();
    if (n > sizeof(buf))
      n = sizeof(buf);
    d.copy_to(buf, n);
    this->rx++;
    if (n < 6 || buf[0] != this->m0 || buf[1] != this->m1) {
      this->foreign++;
      return;
    }
    uint16_t seq = buf[2] | (buf[3] << 8);
    uint16_t sent = buf[4] | (buf[5] << 8);
    if (n != sent) {
      this->trunc++;
      ESP_LOGW("entest", "%s size %u received as %u (seq %u)", this->name, sent, (unsigned) n, seq);
    }
    for (size_t i = 6; i < n; i++)
      if (buf[i] != (uint8_t) (seq + i)) {
        this->bad++;
        ESP_LOGW("entest", "%s bad payload seq=%u size=%u", this->name, seq, (unsigned) n);
        return;
      }
    this->ok++;
    if (n > 250)
      this->big++;
    if (n > this->maxsz)
      this->maxsz = n;
    if (this->last >= 0) {
      if (seq == (uint16_t) this->last) {
        this->dup++;
        return;
      }
      if (seq != (uint16_t) (this->last + 1)) {
        this->gap++;
        ESP_LOGI("entest", "%s seq gap %d -> %u", this->name, (int) this->last, seq);
      }
    }
    this->last = seq;
  }
  bool send(PacketInterface *link, uint16_t size) {
    static uint8_t buf[1600];
    buf[0] = this->m0;
    buf[1] = this->m1;
    buf[2] = this->seq;
    buf[3] = this->seq >> 8;
    buf[4] = size;
    buf[5] = size >> 8;
    for (size_t i = 6; i < size; i++)
      buf[i] = (uint8_t) (this->seq + i);
    if (link->send_to_interface(PacketBuffer(buf, size), {})) {
      this->seq++;
      this->tx++;
      return true;
    }
    this->ref++;
    return false;
  }
  void log() {
    ESP_LOGI("stat", "STAT %s tx=%u ref=%u rx=%u ok=%u bad=%u gap=%u dup=%u trunc=%u big=%u maxsz=%u foreign=%u",
             this->name, this->tx, this->ref, this->rx, this->ok, this->bad, this->gap, this->dup, this->trunc,
             this->big, this->maxsz, this->foreign);
  }
};

inline Stream RAW("raw", 0xA5, 0x5A);
inline Stream P2P("p2p_seq", 0x1E, 0x5A);
inline Stream BCR("bcrel_seq", 0x1E, 0x5B);
// UDP only: the point_to_point false and true links
inline Stream P2PF("p2pf_raw", 0xA5, 0x5B);
inline Stream P2PT("p2pt_raw", 0xA5, 0x5C);
}  // namespace entest
