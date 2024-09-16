#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace iaqcore {

class IAQCore : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_co2(sensor::Sensor *co2) { co2_ = co2; }
  void set_tvoc(sensor::Sensor *tvoc) { tvoc_ = tvoc; }

  void setup() override;
  void update() override;
  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  sensor::Sensor *co2_{nullptr};
  sensor::Sensor *tvoc_{nullptr};

  void publish_nans_();
};

}  // namespace iaqcore
}  // namespace esphome
