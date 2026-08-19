#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF
#ifdef USE_SENSOR

#include <cstdint>

namespace esphome {
namespace sensor {
class Sensor;
}
}  // namespace esphome

namespace esphome {
namespace matter {

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
  enum class Kind : uint8_t {
    UNKNOWN,
    TEMPERATURE,
    HUMIDITY,
    PRESSURE_HPA,  // input in hPa — scale to kPa*10 before writing
    PRESSURE_KPA,  // input in kPa — write kPa*10 directly
    ILLUMINANCE,
    FLOW,
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
  Kind kind_{Kind::UNKNOWN};
};

}  // namespace matter
}  // namespace esphome

#endif  // USE_SENSOR
#endif  // USE_ESP_IDF
