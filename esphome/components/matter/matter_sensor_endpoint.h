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
#ifdef USE_SENSOR

#include <cstdint>

namespace esphome::sensor {
class Sensor;
}  // namespace esphome::sensor

namespace esphome::matter {

// Wraps one ESPHome sensor as a Matter measurement endpoint. Read-only —
// Matter never writes back to a measurement, so no on_matter_write or guard.
//
// Device type chosen from unit_of_measurement (preferred, more reliable than
// device_class since users often omit device_class):
//   - °C  → temperature_sensor     (TemperatureMeasurement, int16 °C×100)
//   - %   → humidity_sensor        (RelativeHumidityMeasurement, uint16 %×100)
//   - hPa → pressure_sensor        (PressureMeasurement, int16 kPa×10 — hPa/10)
//   - kPa → pressure_sensor        (PressureMeasurement, int16 kPa×10)
//   - lx  → light_sensor           (IlluminanceMeasurement, uint16 10000*log10(lux)+1)
//   - m³/h → flow_sensor           (FlowMeasurement, uint16 value×10)
// device_class falls back only when unit_of_measurement doesn't match.
// Sensors with neither a recognized unit nor a recognized device_class are
// skipped with a WARN — Matter has no generic float measurement cluster.
class MatterSensorEndpoint {
 public:
  // Values follow the AGENTS.md rule for enum classes — prefix each with
  // the UPPER_SNAKE_CASE form of the enum name so short tokens like UNKNOWN
  // or FLOW never collide with SDK #define macros.
  enum class Kind : uint8_t {
    KIND_UNKNOWN,
    KIND_TEMPERATURE,
    KIND_HUMIDITY,
    KIND_PRESSURE_HPA,  // input in hPa — scale to kPa*10 before writing
    KIND_PRESSURE_KPA,  // input in kPa — write kPa*10 directly
    KIND_ILLUMINANCE,
    KIND_FLOW,
  };

  explicit MatterSensorEndpoint(sensor::Sensor *s);

  // Detects the Kind — call before setup(). Returns false if no mapping.
  bool detect_kind();

  // Kind picked by detect_kind — inspection helper for logging / component.
  Kind kind() const { return kind_; }

  bool setup();
  void push_initial_state();
  uint16_t endpoint_id() const { return endpoint_id_; }

 protected:
  void report_state_to_fabric_(float state);
  // Publishes the nullable attribute's null value for the current Kind so
  // the fabric renders "no reading" instead of holding a stale last-good
  // value indefinitely when the ESPHome sensor goes unavailable (NaN).
  void report_null_to_fabric_();

  sensor::Sensor *sensor_;
  uint16_t endpoint_id_{0};
  Kind kind_{Kind::KIND_UNKNOWN};
};

}  // namespace esphome::matter

#endif  // USE_SENSOR
#endif  // matter supported variant
#endif  // USE_ESP_IDF
