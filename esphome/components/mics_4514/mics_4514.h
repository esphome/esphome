#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace mics_4514 {

class MICS4514Component : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(carbon_monoxide)
  SUB_SENSOR(nitrogen_dioxide)
  SUB_SENSOR(methane)
  SUB_SENSOR(ethanol)
  SUB_SENSOR(hydrogen)
  SUB_SENSOR(ammonia)

 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

 protected:
  bool warmed_up_{false};
  bool initial_{true};

  float ox_calibration_{0};
  float red_calibration_{0};
};

}  // namespace mics_4514
}  // namespace esphome
