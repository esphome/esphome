#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace max6675 {

class MAX6675Sensor : public sensor::Sensor, public PollingComponent, public spi::SPIDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void update() override;

 protected:
  bool is_device_msb_first() override;

  void read_data_();
};

}  // namespace max6675
}  // namespace esphome
