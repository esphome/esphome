#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF
#ifdef USE_BINARY_SENSOR

#include <cstdint>

// The .cpp includes the switch/binary_sensor and esp_matter headers.
namespace esphome {
namespace binary_sensor {
class BinarySensor;
}
}  // namespace esphome

namespace esphome {
namespace matter {

// Wraps one ESPHome binary_sensor as a Matter endpoint.
//
// Device type picked from device_class:
//   - door / window / opening / garage_door → contact_sensor  (BooleanState cluster)
//   - motion / occupancy / presence         → occupancy_sensor (OccupancySensing cluster)
//   - anything else / unset                 → contact_sensor  (safe default)
//
// This is a read-only wrapper — Matter never writes back to a sensor, so
// there is no on_matter_write() nor an applying_matter_write_ guard.
class MatterBinarySensorEndpoint {
 public:
  enum class DeviceKind : uint8_t {
    CONTACT,
    OCCUPANCY,
  };

  explicit MatterBinarySensorEndpoint(binary_sensor::BinarySensor *bs);

  bool setup();
  void push_initial_state();
  uint16_t endpoint_id() const { return endpoint_id_; }

 protected:
  void report_state_to_fabric_(bool state);
  DeviceKind pick_device_kind_() const;

  binary_sensor::BinarySensor *bs_;
  uint16_t endpoint_id_{0};
  DeviceKind kind_{DeviceKind::CONTACT};
};

}  // namespace matter
}  // namespace esphome

#endif  // USE_BINARY_SENSOR
#endif  // USE_ESP_IDF
