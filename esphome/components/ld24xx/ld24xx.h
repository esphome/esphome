#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

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

// Index of value in a constant table that may live in flash, or none
template<typename T, size_t N> optional<size_t> find_index(const T (&arr)[N], T value) {
  for (size_t i = 0; i < N; i++) {
    T entry;
    progmem_memcpy(&entry, &arr[i], sizeof(entry));
    if (entry == value)
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

#ifdef USE_SENSOR
template<typename T> using SensorWithDedup = sensor::SensorWithDedup<T>;
#endif
}  // namespace esphome::ld24xx
