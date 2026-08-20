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
#endif  // USE_ESP_IDF && USE_MATTER_VARIANT_SUPPORTED
