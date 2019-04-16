#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ct_clamp {

/** This class allows reading the measured current from a CT clamp.
 *
 * It uses the existing `analogRead` methods for doing this.
 *
 * The ESP8266 only has one pin where this can be used: A0
 *
 * The ESP32 has multiple pins that can be used with this component: GPIO32-GPIO39
 * Note you can't use the ADC2 here on the ESP32 because that's already used by WiFi internally.
 * Additionally on the ESP32 you can set an using `set_attenuation`.
 */
class CTClampSensor : public sensor::PollingSensorComponent {
 public:
  explicit CTClampSensor(const std::string &name, uint8_t pin,
    uint32_t calibration, uint32_t sample_size, uint32_t supply_voltage, uint32_t update_interval);

  /// Update CT Clamp sensor values.
  void update() override;
  /// Setup CT Sensor
  void setup() override;
  void dump_config() override;
  /// `HARDWARE_LATE` setup priority.
  float get_setup_priority() const override;

#ifdef ARDUINO_ARCH_ESP8266
  std::string unique_id() override;
#endif

 protected:
  uint8_t pin_;
  uint32_t calibration_;
  uint32_t sample_size_;
  double supply_voltage_;

  int sampleI;
  double Irms,filteredI,offsetI,sumI,sqI;

#ifdef ARDUINO_ARCH_ESP32
#define ADC_BITS    12
#endif
#ifdef ARDUINO_ARCH_ESP8266
#define ADC_BITS    10
#endif
#define ADC_COUNTS  (1<<ADC_BITS)
};

} // namespace ct_clamp
} // namespace esphome
