#pragma once

// Datasheet: https://www.ti.com/lit/ds/symlink/bq27220.pdf
// Technical Reference Manual: https://www.ti.com/lit/ug/sluubd4a/sluubd4a.pdf

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::bq27220 {

class BQ27220Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(voltage)
  SUB_SENSOR(current)
  SUB_SENSOR(battery_level)
  SUB_SENSOR(temperature)
  SUB_SENSOR(remaining_capacity)
  SUB_SENSOR(full_charge_capacity)
  SUB_SENSOR(time_to_empty)
  SUB_SENSOR(state_of_health)

 public:
  void setup() override;
  void update() override;
  void dump_config() override;

 protected:
  bool read_word_(uint8_t reg, uint16_t &value);
  /// Read a standard command register and publish it through the sensor, if one is configured.
  /// A failed read publishes NAN and clears `ok`.
  template<typename F> void publish_(sensor::Sensor *sensor, uint8_t reg, bool &ok, F convert) {
    if (sensor == nullptr)
      return;
    uint16_t raw;
    if (!this->read_word_(reg, raw)) {
      ok = false;
      sensor->publish_state(NAN);
      return;
    }
    sensor->publish_state(convert(raw));
  }
};

}  // namespace esphome::bq27220
