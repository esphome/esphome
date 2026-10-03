#pragma once

#include "esphome/core/defines.h"

// USE_MATTER_VARIANT_SUPPORTED is set by matter's Python to_code() via
// cg.add_define() on the 5 esp-matter-supported ESP32 variants (ESP32,
// S3, C3, C6, H2). It is deliberately NOT declared in
// esphome/core/defines.h — that path is only exercised by clang-tidy and
// static-analysis tools, which do not have esp_matter.h available (the
// SDK is a third-party managed component fetched at build time). Keeping
// the symbol out of defines.h means matter files strip on lint (no
// missing-header errors) but compile normally on real builds where
// Python-side codegen has run. Runtime variant enforcement lives in the
// only_on_variant validator in matter/__init__.py.
#if defined(USE_ESP_IDF) && defined(USE_MATTER_VARIANT_SUPPORTED)
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
#endif  // USE_ESP_IDF && USE_MATTER_VARIANT_SUPPORTED
