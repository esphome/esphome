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

#define LOG_SENSOR_WITH_DEDUP_SAFE(tag, name, sensor) \
  if ((sensor).has_sensor()) { \
    LOG_SENSOR(tag, name, (sensor).get_sensor()); \
  }

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

// Every module frames commands and their answers the same way: this header, a little-endian count of
// the bytes between it and the footer, and this footer. Only the periodic data frame differs.
static constexpr uint8_t HEADER_FOOTER_SIZE = 4;
static constexpr uint8_t CMD_FRAME_HEADER[HEADER_FOOTER_SIZE] = {0xFD, 0xFC, 0xFB, 0xFA};
static constexpr uint8_t CMD_FRAME_FOOTER[HEADER_FOOTER_SIZE] = {0x04, 0x03, 0x02, 0x01};

inline bool validate_header_footer(const uint8_t *header_footer, const uint8_t *buffer) {
  return std::memcmp(header_footer, buffer, HEADER_FOOTER_SIZE) == 0;
}

// Length of the command frame at the start of the buffer, or 0 while its count has not arrived.
// 32 bit so the largest count cannot wrap around to 0.
inline uint32_t cmd_frame_length(const uint8_t *buffer, uint8_t count) {
  if (count < HEADER_FOOTER_SIZE + 2) {
    return 0;
  }
  return HEADER_FOOTER_SIZE + 2 + encode_value_le<uint16_t>(&buffer[HEADER_FOOTER_SIZE]) + HEADER_FOOTER_SIZE;
}

// Drops the first `from` bytes of the buffer and anything after them that does not begin a command or
// data frame header (a partial header at the end is kept). Returns how many of the `count` bytes remain.
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

// Feeds one byte into a frame buffer of N bytes. A frame starts with the command header or `data_header`
// and runs for the length `frame_length(buffer, count)` reports (0 while unknown); footer bytes inside its
// payload cannot end it early. `handle_frame(len)` dispatches a complete frame that fits the buffer and
// returns false when its footer is not where the count says; that frame, or one too long for the buffer,
// is dropped after `on_drop()`. After a frame only the bytes from the next header start on are kept, so
// the buffer always begins with a header. `handle_frame` may overwrite `pos` (the handlers read the frame
// length from it); the splitter works from the count it saved before the call.
template<size_t N, typename LengthFn, typename HandleFn, typename DropFn>
void feed_frame_byte(uint8_t (&buffer)[N], uint8_t &pos, uint8_t byte, const uint8_t *data_header,
                     LengthFn frame_length, HandleFn handle_frame, DropFn on_drop) {
  // Only a frame header opens the buffer; other bytes are dropped until one starts. The first byte picks
  // the header, so the two headers cannot mix.
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
  // A dropped frame can leave whole frames behind it in the buffer, so keep going until the buffer is
  // empty or holds only the start of a frame
  while (true) {
    const uint32_t frame_len = frame_length(buffer, pos);
    if (frame_len == 0 || (frame_len <= N && pos < frame_len)) {
      return;
    }
    const uint8_t total = pos;
    // Whatever follows a handled frame must itself begin with a header before the next pass reads it. A
    // frame whose footer is not where the count says means the stream lost bytes: drop it and keep the
    // bytes from the next header on
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

#ifdef USE_SENSOR
template<typename T> using SensorWithDedup = sensor::SensorWithDedup<T>;
#endif
}  // namespace esphome::ld24xx
