#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include <memory>
#include <span>

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"

#define SUB_SENSOR_WITH_DEDUP(name, dedup_type) \
 protected: \
  ld24xx::SensorWithDedup<dedup_type> *name##_sensor_{nullptr}; \
\
 public: \
  void set_##name##_sensor(sensor::Sensor *sensor) { \
    this->name##_sensor_ = new ld24xx::SensorWithDedup<dedup_type>(sensor); \
  }
#endif

#define LOG_SENSOR_WITH_DEDUP_SAFE(tag, name, sensor) \
  if ((sensor) != nullptr) { \
    LOG_SENSOR(tag, name, (sensor)->sens); \
  }

#define SAFE_PUBLISH_SENSOR(sensor, value) \
  if ((sensor) != nullptr) { \
    (sensor)->publish_state_if_not_dup(value); \
  }

#define SAFE_PUBLISH_SENSOR_UNKNOWN(sensor) \
  if ((sensor) != nullptr) { \
    (sensor)->publish_state_unknown(); \
  }

#define highbyte(val) (uint8_t)((val) >> 8)
#define lowbyte(val) (uint8_t)((val) &0xff)

namespace esphome::ld24xx {

static const char *const UNKNOWN_MAC = "unknown";
static const char *const VERSION_FMT = "%u.%02X.%02X%02X%02X%02X";

// Helper function to format MAC address with stack allocation
// Returns pointer to UNKNOWN_MAC constant or formatted buffer
// Buffer must be exactly 18 bytes (17 for "XX:XX:XX:XX:XX:XX" + null terminator)
inline const char *format_mac_str(const uint8_t *mac_address, std::span<char, 18> buffer) {
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
// Helper class to store a sensor with a deduplicator & publish state only when the value changes
template<typename T> class SensorWithDedup {
 public:
  SensorWithDedup(sensor::Sensor *sens) : sens(sens) {}

  void publish_state_if_not_dup(T state) {
    if (this->publish_dedup.next(state)) {
      this->sens->publish_state(static_cast<float>(state));
    }
  }

  void publish_state_unknown() {
    if (this->publish_dedup.next_unknown()) {
      this->sens->publish_state(NAN);
    }
  }

  sensor::Sensor *sens;
  Deduplicator<T> publish_dedup;
};
#endif
}  // namespace esphome::ld24xx
