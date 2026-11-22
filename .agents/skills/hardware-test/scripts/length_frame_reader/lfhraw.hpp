#pragma once
// The bare reader (#22884): LengthFrameReader fed straight from the UART, with the ld2410 and tuya frame formats and
// checks
#include "lfh.hpp"
#include "esphome/core/frame_reader.h"
#include "esphome/core/hal.h"
#include "esphome/components/uart/uart.h"

namespace lfhraw {
using namespace esphome;
using LdLength = PayloadLengthU16LE<LengthField{.offset = 4, .trailer = 4}>;
using LdData = FrameFormat<FrameHeader<0xF4, 0xF3, 0xF2, 0xF1>, LdLength>;
using LdCmd = FrameFormat<FrameHeader<0xFD, 0xFC, 0xFB, 0xFA>, LdLength>;
using Tuya = FrameFormat<FrameHeader<0x55, 0xAA>, PayloadLengthU16BE<LengthField{.offset = 4, .trailer = 1}>>;
inline LengthFrameReader<50, LdCmd, LdData> ld_reader;
inline LengthFrameReader<255, Tuya> tu_reader;
inline uint32_t last_rx = 0;
inline uint32_t rejected = 0;

inline bool ld_frame(const uint8_t *f, size_t len) {
  static constexpr uint8_t DATA_FOOTER[4] = {0xF8, 0xF7, 0xF6, 0xF5};
  static constexpr uint8_t CMD_FOOTER[4] = {0x04, 0x03, 0x02, 0x01};
  const uint8_t *footer = f[0] == 0xF4 ? DATA_FOOTER : CMD_FOOTER;
  if (len < 12 || memcmp(f + len - 4, footer, 4) != 0) {
    rejected++;
    return false;
  }
  if (f[0] == 0xF4) {
    if (len < 23 || f[7] != 0xAA || f[len - 6] != 0x55 || f[len - 5] != 0x00) {
      rejected++;
      return false;
    }
    lfh::record(f[9] | (f[10] << 8));
  }
  return true;
}
inline bool tu_frame(const uint8_t *f, size_t len) {
  uint8_t s = 0;
  for (size_t i = 0; i + 1 < len; i++)
    s += f[i];
  if (s != f[len - 1]) {
    rejected++;
    return false;
  }
  if (f[3] == 0x07 && len == 15 && f[6] == 0x65)
    lfh::record(static_cast<int32_t>((f[10] << 24) | (f[11] << 16) | (f[12] << 8) | f[13]));
  return true;
}
inline void poll(uart::UARTComponent *bus) {
  uint8_t c;
  const uint32_t now = (esphome::millis) ();
  while (bus->available() > 0 && bus->read_byte(&c)) {
    ld_reader.feed(c, ld_frame);
    tu_reader.feed(c, tu_frame);
    last_rx = now;
  }
  if (now - last_rx > 300) {
    if (ld_reader.length() > 0)
      ld_reader.flush(ld_frame);
    if (tu_reader.length() > 0)
      tu_reader.flush(tu_frame);
  }
}
}  // namespace lfhraw
