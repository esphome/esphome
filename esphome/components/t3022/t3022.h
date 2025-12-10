#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace t3022 {

class T3022Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_co2_sensor(sensor::Sensor *co2) { co2_sensor_ = co2; }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  sensor::Sensor *co2_sensor_{nullptr};

  // Helper methods for I2C communication
  bool send_command_(const uint8_t *command, size_t len);
  bool read_response_(uint8_t *data, size_t len);
  bool check_status_();
  void read_co2_value_();
  void publish_nan_();

  // Timing constants (in milliseconds)
  static constexpr uint8_t READ_DELAY = 5;         // Delay between I2C write & read
  static constexpr uint16_t MEASURE_DELAY = 2250;  // Delay between measure and read PPM (minimum recommended)
};

}  // namespace t3022
}  // namespace esphome
