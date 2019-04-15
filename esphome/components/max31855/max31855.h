#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace max31855 {

class MAX31855Sensor : public sensor::PollingSensorComponent, public spi::SPIDevice {
 public:
  MAX31855Sensor(const std::string &name, uint32_t update_interval);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void update() override;

 protected:
  bool is_device_msb_first() override;

  void read_data_();
};

}  // namespace max31855
}  // namespace esphome
