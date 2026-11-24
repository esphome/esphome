#pragma once
// Fake DLE/STX/ETX bus device for rs485_frame: sum16 big endian, header inclusive. Stuffing is
// DLE 00 (escape_byte) or DLE DLE (double). Logs each frame the hub sends with arrival times.
#include <vector>
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"

namespace simrs {
static const char *const TAG = "sim";
inline esphome::uart::UARTComponent *bus = nullptr;
inline bool double_mode = false;

inline std::vector<uint8_t> frame(std::vector<uint8_t> body, bool bad_crc = false) {
  uint16_t sum = esphome::sum16_bytes(body.data(), body.size(), 0x10 + 0x02);
  if (bad_crc)
    sum ^= 0x0101;
  body.push_back(sum >> 8);
  body.push_back(sum & 0xFF);
  std::vector<uint8_t> out{0x10, 0x02};
  for (uint8_t b : body) {
    out.push_back(b);
    if (b == 0x10)
      out.push_back(double_mode ? 0x10 : 0x00);
  }
  out.push_back(0x10);
  out.push_back(0x03);
  return out;
}

inline uint32_t last_send_end = 0;  // when the last byte we wrote should have left the wire
inline void send(const char *label, const std::vector<uint8_t> &bytes) {
  char hex[600];
  esphome::format_hex_to(hex, sizeof(hex), bytes.data(), bytes.size());
  uint32_t now = millis();
  last_send_end = now + (bytes.size() * 10 * 1000 + 9599) / 9600;
  ESP_LOGI(TAG, "TX t=%u wire_end=%u %s (%u B) %s", (unsigned) now, (unsigned) last_send_end, label,
           (unsigned) bytes.size(), hex);
  bus->write_array(bytes.data(), bytes.size());
}

// Frames the hub sends, split on DLE STX / DLE ETX, with the poll time of first and last byte.
inline void poll() {
  static uint8_t buf[600];
  static size_t len = 0;
  static uint32_t start = 0, prev_end = 0;
  static bool after_dle = false;
  static unsigned n = 0;
  uint8_t b;
  while (bus->available() && bus->read_byte(&b)) {
    uint32_t now = millis();
    if (len == 0)
      start = now;
    if (len < sizeof(buf))
      buf[len++] = b;
    if (after_dle) {
      after_dle = false;
      if (b == 0x03) {
        char hex[1210];
        esphome::format_hex_to(hex, sizeof(hex), buf, len);
        ESP_LOGI(TAG, "RXF %u start=%u end=%u len=%u gap_from_prev_end=%d since_our_wire_end=%d %s", ++n,
                 (unsigned) start, (unsigned) now, (unsigned) len, prev_end ? (int) (start - prev_end) : -1,
                 (int) (start - last_send_end), hex);
        prev_end = now;
        len = 0;
      }
    } else if (b == 0x10) {
      after_dle = true;
    }
  }
}

inline std::vector<uint8_t> cat(std::vector<uint8_t> a, const std::vector<uint8_t> &b) {
  a.insert(a.end(), b.begin(), b.end());
  return a;
}
inline std::vector<uint8_t> filled(std::vector<uint8_t> head, size_t n, uint8_t v) {
  head.resize(n, v);
  return head;
}
}  // namespace simrs
