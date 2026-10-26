#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <span>

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor_with_dedup.h"
#endif

#define SAFE_PUBLISH_SENSOR(sensor, value) (sensor).publish_state_if_not_dup(value)

#define SAFE_PUBLISH_SENSOR_UNKNOWN(sensor) (sensor).publish_state_unknown()

#define highbyte(val) (uint8_t) ((val) >> 8)
#define lowbyte(val) (uint8_t) ((val) & 0xff)

namespace esphome::ld24xx {

// Helper to find index of value in constexpr array
template<size_t N> optional<size_t> find_index(const uint32_t (&arr)[N], uint32_t value) {
  for (size_t i = 0; i < N; i++) {
    if (arr[i] == value)
      return i;
  }
  return {};
}

static const char *const UNKNOWN_MAC = "unknown";
static const char *const VERSION_FMT = "%u.%02X.%02X%02X%02X%02X";

// Helper function to format MAC address with stack allocation
// Returns pointer to UNKNOWN_MAC constant or formatted buffer
inline const char *format_mac_str(const uint8_t *mac_address, std::span<char, MAC_ADDRESS_PRETTY_BUFFER_SIZE> buffer) {
  if (mac_address_is_valid(mac_address)) {
    format_mac_addr_upper(mac_address, buffer.data());
    return buffer.data();
  }
  return UNKNOWN_MAC;
}

// Helper function to format firmware version with stack allocation
// Buffer must be exactly 20 bytes (format: "x.xxXXXXXX" fits in 11 + null terminator, 20 for safety)
inline void format_version_str(const uint8_t *version, std::span<char, 20> buffer) {
  snprintf(buffer.data(), buffer.size(), VERSION_FMT, version[1], version[0], version[5], version[4], version[3],
           version[2]);
}

// Commands and their answers share this header, a little-endian byte count and this footer
static constexpr uint8_t HEADER_FOOTER_SIZE = 4;
static constexpr uint8_t CMD_FRAME_HEADER[HEADER_FOOTER_SIZE] = {0xFD, 0xFC, 0xFB, 0xFA};
static constexpr uint8_t CMD_FRAME_FOOTER[HEADER_FOOTER_SIZE] = {0x04, 0x03, 0x02, 0x01};

inline bool validate_header_footer(const uint8_t *header_footer, const uint8_t *buffer) {
  return std::memcmp(header_footer, buffer, HEADER_FOOTER_SIZE) == 0;
}

// Whole command frame length, or 0 until the count arrives; 32 bit so 0xFFFF cannot wrap to 0
inline uint32_t cmd_frame_length(const uint8_t *buffer, uint8_t count) {
  if (count < HEADER_FOOTER_SIZE + 2) {
    return 0;
  }
  return HEADER_FOOTER_SIZE + 2 + encode_value_le<uint16_t>(&buffer[HEADER_FOOTER_SIZE]) + HEADER_FOOTER_SIZE;
}

// Drops the first `from` bytes and anything after them that does not begin a header; returns the bytes kept
inline uint8_t drop_to_next_header(uint8_t *buffer, uint8_t count, uint8_t from, const uint8_t *data_header) {
  for (; from < count; from++) {
    const size_t n = std::min<size_t>(HEADER_FOOTER_SIZE, count - from);
    if (std::memcmp(&buffer[from], CMD_FRAME_HEADER, n) == 0 || std::memcmp(&buffer[from], data_header, n) == 0) {
      std::memmove(buffer, &buffer[from], count - from);
      return count - from;
    }
  }
  return 0;
}

// Feeds one byte into a frame buffer. A frame opens with the command header or `data_header` and ends at
// `frame_length(buffer, count)` bytes (0 while unknown); `handle_frame(len)` returns false for a footer out
// of place. A dropped or oversized frame calls `on_drop()`. The handler may overwrite `pos`; the splitter
// works from the count it saved.
template<size_t N, typename LengthFn, typename HandleFn, typename DropFn>
void feed_frame_byte(uint8_t (&buffer)[N], uint8_t &pos, uint8_t byte, const uint8_t *data_header,
                     LengthFn frame_length, HandleFn handle_frame, DropFn on_drop) {
  // The first byte picks the header; bytes that do not continue it are dropped
  if (pos > 0 && pos < HEADER_FOOTER_SIZE) {
    const uint8_t *header = buffer[0] == data_header[0] ? data_header : CMD_FRAME_HEADER;
    if (byte != header[pos]) {
      pos = 0;
    }
  }
  if (pos == 0 && byte != data_header[0] && byte != CMD_FRAME_HEADER[0]) {
    return;
  }
  buffer[pos++] = byte;
  // Drain every whole frame left in the buffer
  while (true) {
    const uint32_t frame_len = frame_length(buffer, pos);
    if (frame_len == 0 || (frame_len <= N && pos < frame_len)) {
      return;
    }
    const uint8_t total = pos;
    // Keep only what begins a header: after the frame, or after its first byte when it is dropped
    uint8_t from = 1;
    if (frame_len <= N && handle_frame(static_cast<uint8_t>(frame_len))) {
      from = frame_len;
    } else {
      on_drop();
    }
    pos = drop_to_next_header(buffer, total, from, data_header);
    if (pos == 0) {
      return;
    }
  }
}

}  // namespace esphome::ld24xx
