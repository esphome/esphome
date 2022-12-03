#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace sn74hc165 {

class SN74HC165GPIOBinarySensor : public binary_sensor::BinarySensor {
 public:
  void set_pin(const uint8_t pin) { this->pin_ = pin;}
  void process(const uint32_t data);
 protected:
  uint8_t pin_;  
};

class SN74HC165Component : public PollingComponent {
 public:
  SN74HC165Component() = default;

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; };
  void register_input(SN74HC165GPIOBinarySensor* sensor);
  void set_data_pin(GPIOPin *pin) { data_pin_ = pin; }
  void set_clock_pin(GPIOPin *pin) { clock_pin_ = pin; }
  void set_latch_pin(GPIOPin *pin) { latch_pin_ = pin; }
  void set_sr_count(uint8_t count) { sr_count_ = count; }
  void set_scan_rate(uint32_t scan_rate) { this->set_update_interval(scan_rate); }

 protected:

  void update() override;
  
  uint32_t read_gpio_();
  GPIOPin *data_pin_;
  GPIOPin *clock_pin_;
  GPIOPin *latch_pin_;  
  uint8_t sr_count_;

  std::vector<SN74HC165GPIOBinarySensor*> sensors_;
};




}  // namespace sn74hc165
}  // namespace esphome
