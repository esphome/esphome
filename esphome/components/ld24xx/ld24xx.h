#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/frame_reader.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/progmem.h"

#include <cstring>
#include <span>

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor_with_dedup.h"
#endif

#define SAFE_PUBLISH_SENSOR(sensor, value) (sensor).publish_state_if_not_dup(value)

#define SAFE_PUBLISH_SENSOR_UNKNOWN(sensor) (sensor).publish_state_unknown()

#define highbyte(val) (uint8_t) ((val) >> 8)
#define lowbyte(val) (uint8_t) ((val) & 0xff)

namespace esphome::ld24xx {

// Index of value in a constant table that may live in flash, or none
template<typename T, size_t N> optional<size_t> find_index(const T (&arr)[N], T value) {
  static_assert(sizeof(T) == 1 || sizeof(T) == 4, "flash tables are read as bytes or aligned 32 bit words");
  for (size_t i = 0; i < N; i++) {
    T entry;
    if constexpr (sizeof(T) == 1) {
      entry = progmem_read_byte(reinterpret_cast<const uint8_t *>(&arr[i]));
    } else {
      entry = arr[i];  // an aligned 32 bit read from flash is safe on ESP8266
    }
    if (entry == value)
      return i;
  }
  return {};
}

// Formats the MAC address into the buffer, or "unknown" when it is not valid
inline const char *format_mac_str(const uint8_t *mac_address, std::span<char, MAC_ADDRESS_PRETTY_BUFFER_SIZE> buffer) {
  // mac_address_is_valid() rejects the multicast bit, which radar Bluetooth MACs may have set; only all-00 and all-FF
  // mean no address
  uint8_t any_set = 0x00;
  uint8_t all_set = 0xFF;
  for (size_t i = 0; i < MAC_ADDRESS_SIZE; i++) {
    any_set |= mac_address[i];
    all_set &= mac_address[i];
  }
  if (any_set != 0x00 && all_set != 0xFF) {
    format_mac_addr_upper(mac_address, buffer.data());
  } else {
    ESPHOME_strncpy_P(buffer.data(), ESPHOME_PSTR("unknown"), buffer.size());
  }
  return buffer.data();
}

// MAC address the module uses when Bluetooth is disabled
static constexpr uint8_t NO_MAC[MAC_ADDRESS_SIZE] PROGMEM = {0x08, 0x05, 0x04, 0x03, 0x02, 0x01};

// Stores the radar's reported MAC, or clears it when the radar reports Bluetooth off; returns whether Bluetooth is on
inline bool store_mac(const uint8_t *reported, uint8_t *mac_address) {
  uint8_t no_mac[MAC_ADDRESS_SIZE];
  progmem_memcpy(no_mac, NO_MAC, MAC_ADDRESS_SIZE);
  if (std::memcmp(reported, no_mac, MAC_ADDRESS_SIZE) == 0) {
    std::memset(mac_address, 0, MAC_ADDRESS_SIZE);
    return false;
  }
  std::memcpy(mac_address, reported, MAC_ADDRESS_SIZE);
  return true;
}

// Helper function to format firmware version with stack allocation
// Buffer must be exactly 20 bytes (format: "x.xxXXXXXX" fits in 11 + null terminator, 20 for safety)
inline void format_version_str(const uint8_t *version, std::span<char, 20> buffer) {
  ESPHOME_snprintf_P(buffer.data(), buffer.size(), ESPHOME_PSTR("%u.%02X.%02X%02X%02X%02X"), version[1], version[0],
                     version[5], version[4], version[3], version[2]);
}

// Commands and their answers share this header, a little-endian byte count and this footer; periodic data frames have
// their own header and footer
static constexpr uint8_t HEADER_FOOTER_SIZE = 4;
static constexpr uint8_t CMD_FRAME_HEADER[HEADER_FOOTER_SIZE] PROGMEM = {0xFD, 0xFC, 0xFB, 0xFA};
static constexpr uint8_t CMD_FRAME_FOOTER[HEADER_FOOTER_SIZE] PROGMEM = {0x04, 0x03, 0x02, 0x01};
static constexpr uint8_t DATA_FRAME_FOOTER[HEADER_FOOTER_SIZE] PROGMEM = {0xF8, 0xF7, 0xF6, 0xF5};

// Header (4), payload length (2, LE), payload, footer (4)
using PayloadLength = PayloadLengthU16LE<LengthField{.offset = HEADER_FOOTER_SIZE, .trailer = HEADER_FOOTER_SIZE}>;
using CmdFrameHeader = FrameHeader<0xFD, 0xFC, 0xFB, 0xFA>;
using DataFrameHeader = FrameHeader<0xF4, 0xF3, 0xF2, 0xF1>;
using CmdFrameFormat = FrameFormat<CmdFrameHeader, PayloadLength>;
using DataFrameFormat = FrameFormat<DataFrameHeader, PayloadLength>;

// Whether a command or periodic data frame of at least HEADER_FOOTER_SIZE bytes ends in its footer
inline bool has_footer(const uint8_t *frame, size_t len) {
  uint8_t footer[HEADER_FOOTER_SIZE];
  progmem_memcpy(footer, frame[0] == DataFrameFormat::FIRST ? DATA_FRAME_FOOTER : CMD_FRAME_FOOTER, sizeof(footer));
  return std::memcmp(&frame[len - HEADER_FOOTER_SIZE], footer, sizeof(footer)) == 0;
}

}  // namespace esphome::ld24xx
