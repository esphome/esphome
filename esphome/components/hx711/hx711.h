#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace hx711 {

enum HX711Gain {
  HX711_GAIN_128 = 1,
  HX711_GAIN_32 = 2,
  HX711_GAIN_64 = 3,
};

class HX711Sensor : public sensor::PollingSensorComponent {
 public:
  HX711Sensor(const std::string &name, GPIOPin *dout, GPIOPin *sck, uint32_t update_interval);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_gain(HX711Gain gain);

 protected:
  bool read_sensor_(uint32_t *result);

  GPIOPin *dout_pin_;
  GPIOPin *sck_pin_;
  HX711Gain gain_{HX711_GAIN_128};
};

}  // namespace hx711
}  // namespace esphome
