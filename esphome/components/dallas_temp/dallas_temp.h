#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/one_wire/one_wire.h"

namespace esphome {
namespace dallas_temp {

class DallasTemperatureSensor : public PollingComponent, public sensor::Sensor, public one_wire::OneWireDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  /// Set the resolution for this sensor.
  void set_resolution(uint8_t resolution) { this->resolution_ = resolution; }

 protected:
  uint8_t resolution_;
  uint8_t scratch_pad_[9] = {0};

  /// Get the number of milliseconds we have to wait for the conversion phase.
  uint16_t millis_to_wait_for_conversion_() const;
  bool read_scratch_pad_();
  void read_scratch_pad_int_();
  bool check_scratch_pad_();
  float get_temp_c_();
};

}  // namespace dallas_temp
}  // namespace esphome
