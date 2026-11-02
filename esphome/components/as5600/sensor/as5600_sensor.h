#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/preferences.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/as5600/as5600.h"

namespace esphome::as5600 {

class AS5600Sensor final : public PollingComponent, public Parented<AS5600Component>, public sensor::Sensor {
  SUB_SENSOR(raw_position)
  SUB_SENSOR(gain)
  SUB_SENSOR(magnitude)
  SUB_SENSOR(status)

 public:
  void update() override;
  void dump_config() override;

  void set_out_of_range_mode(OutRangeMode oor_mode) { this->out_of_range_mode_ = oor_mode; }
  OutRangeMode get_out_of_range_mode() { return this->out_of_range_mode_; }

 protected:
  OutRangeMode out_of_range_mode_{OUT_RANGE_MODE_MIN_MAX};
};

}  // namespace esphome::as5600
