#pragma once
// Fake DLE/STX/ETX bus device for rs485_frame: builds sum16 big endian, header inclusive frames with 0x10 0x00
// stuffing.
#include <vector>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"

namespace simrs {
static const char *const TAG = "sim";
inline esphome::uart::UARTComponent *bus = nullptr;

inline std::vector<uint8_t> frame(std::vector<uint8_t> body, bool bad_crc = false) {
  uint16_t sum = 0x10 + 0x02;
  for (uint8_t b : body)
    sum += b;
  if (bad_crc)
    sum ^= 0x0101;
  body.push_back(sum >> 8);
  body.push_back(sum & 0xFF);
  std::vector<uint8_t> out{0x10, 0x02};
  for (uint8_t b : body) {
    out.push_back(b);
    if (b == 0x10)
      out.push_back(0x00);
  }
  out.push_back(0x10);
  out.push_back(0x03);
  return out;
}

inline void send(const char *label, const std::vector<uint8_t> &bytes) {
  char hex[600];
  esphome::format_hex_to(hex, sizeof(hex), bytes.data(), bytes.size() > 290 ? 290 : bytes.size());
  ESP_LOGI(TAG, "TX %s (%u B) %s", label, (unsigned) bytes.size(), hex);
  bus->write_array(bytes.data(), bytes.size());
}

// Collect what the hub transmits; log once the line is quiet.
inline void poll() {
  static uint8_t buf[256];
  static size_t len = 0;
  static uint32_t last = 0;
  while (bus->available() && len < sizeof(buf)) {
    bus->read_byte(&buf[len++]);
    last = esphome::millis();
  }
  if (len && esphome::millis() - last > 30) {
    char hex[520];
    esphome::format_hex_to(hex, sizeof(hex), buf, len);
    ESP_LOGI(TAG, "RX (%u B) %s", (unsigned) len, hex);
    len = 0;
  }
}

inline std::vector<uint8_t> cat(std::vector<uint8_t> a, const std::vector<uint8_t> &b) {
  a.insert(a.end(), b.begin(), b.end());
  return a;
}
}  // namespace simrs
