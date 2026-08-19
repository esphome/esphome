#pragma once

#include "esphome/core/defines.h"

// esp-matter 1.6.0 only supports these ESP32 variants. Strip the whole
// TU on any other target (P4, S2, C2, C5, C61, H4, H21, S31) so clang-tidy
// jobs for those variants — which grep this file in via USE_WIFI /
// USE_ETHERNET — don't try to compile against an esp_matter.h that upstream
// never ships for those chips. Runtime builds are already rejected by the
// only_on_variant config validator in matter/__init__.py; this guard is the
// static-analysis mirror of the same restriction.
#ifdef USE_ESP_IDF
#if defined(USE_ESP32_VARIANT_ESP32) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32C3) || \
    defined(USE_ESP32_VARIANT_ESP32C6) || defined(USE_ESP32_VARIANT_ESP32H2)
#ifdef USE_BINARY_SENSOR

#include <cstdint>

// The .cpp includes the switch/binary_sensor and esp_matter headers.
namespace esphome::binary_sensor {
class BinarySensor;
}  // namespace esphome::binary_sensor

namespace esphome::matter {

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
  // Values follow the AGENTS.md rule for enum classes — prefix each with
  // the UPPER_SNAKE_CASE form of the enum name so bare tokens like CONTACT
  // never collide with SDK #define macros the preprocessor would substitute
  // before the compiler sees them.
  enum class DeviceKind : uint8_t {
    DEVICE_KIND_CONTACT,
    DEVICE_KIND_OCCUPANCY,
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
  DeviceKind kind_{DeviceKind::DEVICE_KIND_CONTACT};
};

}  // namespace esphome::matter

#endif  // USE_BINARY_SENSOR
#endif  // matter supported variant
#endif  // USE_ESP_IDF
