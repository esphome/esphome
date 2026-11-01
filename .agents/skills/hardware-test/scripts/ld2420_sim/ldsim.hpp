#pragma once
// Fake LD2420 on the UART0 loopback: answers each command frame the hub sends with a success ack written right
// behind it, and builds energy frames and simple-mode lines for injection.
// The hub also reads its own echoed command. A read register command carries 00 00 where an ack has its error word,
// so the echo counts as that command's ack and every real ack lands one command late. The firmware version is
// therefore still v0.0.0 when setup() publishes the Firmware text sensor (a real module does not echo); the config
// dump and the Simple Mode check see the simulated version.
#include <cstring>
#include <string>
#include <vector>
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/log.h"

namespace ldsim {
static uint32_t n_cmds, n_acks;

inline void put16(std::vector<uint8_t> &v, uint16_t x) {
  v.push_back(x & 0xFF);
  v.push_back(x >> 8);
}

inline void on_tx(esphome::uart::UARTComponent *bus, const std::vector<uint8_t> &raw, const char *fw) {
  std::vector<uint8_t> b(raw);  // the debugger buffer changes when the ack is written
  static const uint8_t HDR[4] = {0xFD, 0xFC, 0xFB, 0xFA};
  size_t p = std::string::npos;
  for (size_t i = 0; i + 4 <= b.size(); i++)
    if (memcmp(&b[i], HDR, 4) == 0)
      p = i;
  if (p == std::string::npos || b.size() < p + 12)
    return;
  b.erase(b.begin(), b.begin() + p);
  if (b[7] != 0x00)
    return;  // our own ack
  uint16_t len = b[4] | (b[5] << 8);
  uint16_t cmd = b[6] | (b[7] << 8);
  n_cmds++;
  if (cmd == 0x0068)
    return;  // restart has no reply
  std::vector<uint8_t> data;
  uint16_t nargs = len >= 2 ? (len - 2) / 2 : 0;
  if (cmd == 0x00FF) {
    put16(data, 2);
    put16(data, 0x40);
  } else if (cmd == 0x0000) {
    put16(data, strlen(fw));
    for (const char *c = fw; *c; c++)
      data.push_back(*c);
  } else if (cmd == 0x0002) {  // read registers: min gate 1, max gate 12, timeout 120
    for (uint16_t i = 0; i < nargs; i++) {
      uint16_t a = b[8 + 2 * i] | (b[9 + 2 * i] << 8);
      put16(data, a == 0 ? 1 : a == 1 ? 12 : a == 4 ? 120 : 0);
    }
  } else if (cmd == 0x0008) {  // read gate parameters, 4 bytes each
    for (uint16_t i = 0; i < nargs; i++) {
      put16(data, 250);
      put16(data, 0);
    }
  }
  std::vector<uint8_t> a = {0xFD, 0xFC, 0xFB, 0xFA};
  put16(a, 4 + data.size());
  put16(a, cmd | 0x0100);
  put16(a, 0);  // no error
  a.insert(a.end(), data.begin(), data.end());
  a.insert(a.end(), {0x04, 0x03, 0x02, 0x01});
  bus->write_array(a.data(), a.size());
  n_acks++;
}

inline void energy(esphome::uart::UARTComponent *bus, uint8_t presence, uint16_t range) {
  std::vector<uint8_t> f = {0xF4, 0xF3, 0xF2, 0xF1};
  put16(f, 1 + 2 + 32);
  f.push_back(presence);
  put16(f, range);
  for (int g = 0; g < 16; g++)
    put16(f, 100 + g);
  f.insert(f.end(), {0xF8, 0xF7, 0xF6, 0xF5});
  bus->write_array(f.data(), f.size());
}

inline void line(esphome::uart::UARTComponent *bus, const char *s) {
  bus->write_array(reinterpret_cast<const uint8_t *>(s), strlen(s));
  bus->write_array(reinterpret_cast<const uint8_t *>("\r\n"), 2);
}
}  // namespace ldsim
