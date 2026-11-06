#pragma once
// Fake HLK-LD6002B on the far end of a cross-wired UART pair: ACKs every command, logs the exact bytes it received,
// and injects TinyFrame reports (good and damaged) on request.
#include <cstring>
#include <vector>
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace ld6sim {
using esphome::uart::UARTComponent;
static const char *const TAG = "ld6sim";

inline uint8_t ixor(const std::vector<uint8_t> &v, size_t from, size_t to) {
  uint8_t x = 0;
  for (size_t i = from; i < to; i++)
    x ^= v[i];
  return (uint8_t) ~x;
}
inline std::vector<uint8_t> frame(uint16_t id, uint16_t type, const std::vector<uint8_t> &data) {
  std::vector<uint8_t> f = {0x01,
                            (uint8_t) (id >> 8),
                            (uint8_t) id,
                            (uint8_t) (data.size() >> 8),
                            (uint8_t) data.size(),
                            (uint8_t) (type >> 8),
                            (uint8_t) type};
  f.push_back(ixor(f, 0, 7));
  if (!data.empty()) {
    f.insert(f.end(), data.begin(), data.end());
    f.push_back(ixor(data, 0, data.size()));
  }
  return f;
}
inline void put_f(std::vector<uint8_t> &d, float v) {
  uint32_t u;
  memcpy(&u, &v, 4);
  for (int i = 0; i < 4; i++)
    d.push_back(u >> (8 * i));
}
inline void put_u(std::vector<uint8_t> &d, uint32_t u) {
  for (int i = 0; i < 4; i++)
    d.push_back(u >> (8 * i));
}
// Target report with one target at x (the marker), y 2.0, z 0.5, cluster 7; count 0 when x < 0.
inline std::vector<uint8_t> target(float x) {
  std::vector<uint8_t> d;
  if (x < 0) {
    put_u(d, 0);
    return frame(0x1234, 0x0A04, d);
  }
  put_u(d, 1);
  put_f(d, x);
  put_f(d, 2.0f);
  put_f(d, 0.5f);
  put_u(d, 3);
  put_u(d, 7);
  return frame(0x1234, 0x0A04, d);
}

// Target report with n targets (x = base + i, cluster 7 + i); count field may differ from the entries sent.
inline std::vector<uint8_t> targets_n(int n, float base, int count_field = -1) {
  std::vector<uint8_t> d;
  put_u(d, count_field < 0 ? n : count_field);
  for (int i = 0; i < n; i++) {
    put_f(d, base + i);
    put_f(d, 2.0f);
    put_f(d, 0.5f);
    put_u(d, 3);
    put_u(d, 7 + i);
  }
  return frame(0x1234, 0x0A04, d);
}
inline std::vector<uint8_t> areas4(uint32_t a, uint32_t b, uint32_t c, uint32_t e) {
  std::vector<uint8_t> d;
  put_u(d, a);
  put_u(d, b);
  put_u(d, c);
  put_u(d, e);
  return frame(0x1234, 0x0A0A, d);
}

struct Module {
  UARTComponent *u{nullptr};
  uint8_t buf[64];
  uint8_t n{0};
  uint32_t acks{0};

  void send(const char *name, const std::vector<uint8_t> &b) {
    ESP_LOGI(TAG, "INJECT %s (%u bytes)", name, (unsigned) b.size());
    this->u->write_array(b.data(), b.size());
  }
  // Byte by byte with a pause, so the component sees the frame in many reads.
  void send_slow(const char *name, const std::vector<uint8_t> &b) {
    ESP_LOGI(TAG, "INJECT %s (%u bytes, 1 byte per 2 ms)", name, (unsigned) b.size());
    for (uint8_t x : b) {
      this->u->write_byte(x);
      this->u->flush();
      esphome::delay(2);
    }
  }
  // Answers the queries with fixed reports, as the module would after its ACK.
  void reply(uint16_t type, const uint8_t *data, uint16_t len) {
    std::vector<uint8_t> d;
    if (type == 0xFFFF) {
      d = {0, 1, 2, 3};
      this->out(0xFFFF, d);
      return;
    }
    if (type != 0x0201 || len < 4)
      return;
    switch (data[0]) {
      case 0x0D:
        this->out(0x0A0E, {1});
        break;
      case 0x11:
        this->out(0x0A0F, {2});
        break;
      case 0x15:
        this->out(0x0A11, {1});
        break;
      case 0x12:
        put_f(d, -1.5f);
        put_f(d, 2.5f);
        this->out(0x0A10, d);
        break;
      case 0x16:
      case 0x17:
      case 0x18:
        this->out(0x0A12, {(uint8_t) (data[0] == 0x16 ? 1 : 0)});
        break;
      case 0x19:
        put_u(d, 30);
        this->out(0x0A13, d);
        break;
      case 0x05:
        put_u(d, 20);
        this->out(0x0A0D, d);
        break;
      case 0x02:
        for (int kind = 0; kind < 2; kind++) {
          std::vector<uint8_t> a;
          for (int i = 0; i < 4; i++)
            for (int axis = 0; axis < 6; axis++)
              put_f(a, (kind + 1) * 10 + i + axis * 0.25f - (axis % 2 == 0 ? 3.0f : 0.0f));
          this->out(kind == 0 ? 0x0A0B : 0x0A0C, a);
        }
        break;
      default:
        break;
    }
  }
  void out(uint16_t type, const std::vector<uint8_t> &d) {
    auto f = frame(0x2000 + this->acks, type, d);
    ESP_LOGI(TAG, "MOD TX type=%04X len=%u", type, (unsigned) d.size());
    this->u->write_array(f.data(), f.size());
  }
  // Collect the component's frames (SOF..checksums) and ACK each with an empty frame of the same type.
  void poll() {
    uint8_t c;
    while (this->u->available() && this->u->read_byte(&c)) {
      if (this->n == 0 && c != 0x01)
        continue;
      this->buf[this->n++] = c;
      if (this->n >= 8) {
        uint16_t len = (this->buf[3] << 8) | this->buf[4];
        size_t total = 8 + (len ? len + 1 : 0);
        if (total > sizeof(this->buf)) {
          this->n = 0;
          continue;
        }
        if (this->n == total) {
          char hex[3 * 64 + 1] = {0};
          for (size_t i = 0; i < total; i++)
            sprintf(hex + 3 * i, "%02X ", this->buf[i]);
          uint16_t type = (this->buf[5] << 8) | this->buf[6];
          ESP_LOGI(TAG, "MOD RX type=%04X len=%u: %s", type, len, hex);
          std::vector<uint8_t> ack = frame((this->buf[1] << 8) | this->buf[2], type, {});
          this->u->write_array(ack.data(), ack.size());
          this->acks++;
          this->reply(type, len ? this->buf + 8 : nullptr, len);
          this->n = 0;
        }
      }
    }
  }
};
static Module mod;
}  // namespace ld6sim
