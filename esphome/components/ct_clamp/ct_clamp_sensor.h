#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ct_clamp {

class CTClampSensor : public sensor::Sensor, public PollingComponent {
 public:
  /// Update CT Clamp sensor values.
  void update() override;
  /// Setup CT Sensor
  void setup() override;
  void dump_config() override;
  /// `HARDWARE_LATE` setup priority.
  float get_setup_priority() const override;

  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_calibration(uint32_t calibration) { this->calibration_ = calibration; }
  void set_sample_size(uint32_t sample_size) { this->sample_size_ = sample_size; }
  void set_supply_voltage(double supply_voltage) { this->supply_voltage_ = supply_voltage; }

#ifdef ARDUINO_ARCH_ESP8266
  std::string unique_id() override;
#endif

 protected:
  uint8_t pin_;
  uint32_t calibration_;
  uint32_t sample_size_;
  double supply_voltage_;

  int sample_i_;
  double irms_,filtered_i_,offset_i_,sum_i_,sq_i_;

#ifdef ARDUINO_ARCH_ESP32
#define ADC_BITS 12
#endif
#ifdef ARDUINO_ARCH_ESP8266
#define ADC_BITS 10
#endif
#define ADC_COUNTS (1 << ADC_BITS)
};

}  // namespace ct_clamp
}  // namespace esphome
