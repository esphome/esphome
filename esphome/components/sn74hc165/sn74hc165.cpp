#include "sn74hc165.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sn74hc165 {

static const char *const TAG = "sn74hc165";

void SN74HC165Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SN74HC165...");

  // initialize output pins
  this->clock_pin_->setup();
  this->data_pin_->setup();
  this->latch_pin_->setup();
  this->clock_pin_->digital_write(true);
  this->data_pin_->digital_write(false);
  this->latch_pin_->digital_write(true); 
}

void SN74HC165Component::dump_config() { 
  ESP_LOGCONFIG(TAG, "SN74HC165:"); 
  LOG_PIN("  Clock Pin: ", this->clock_pin_);
  LOG_PIN("  Data Pin: ", this->data_pin_);
  LOG_PIN("  Latch Pin: ", this->latch_pin_);
  ESP_LOGCONFIG(TAG, "  Scan rate: %u", this->get_update_interval());  
}


void SN74HC165Component::register_input(SN74HC165GPIOBinarySensor* sensor){
  this->sensors_.push_back(sensor);
}

void SN74HC165Component::update(){
  const uint32_t input_values = this->read_gpio_();
  for(auto& sensor: this->sensors_){
    sensor->process(input_values);
  }
}

uint32_t SN74HC165Component::read_gpio_() {  
  this->clock_pin_->digital_write(true);
  this->latch_pin_->digital_write(false);
  this->latch_pin_->digital_write(true);
  uint32_t input_bits{0};
  const uint8_t loop_counter = this->sr_count_ * 8;
  for (uint8_t i = 0; i < loop_counter; i++) { 
    this->clock_pin_->digital_write(true);
    const bool value = this->data_pin_->digital_read();
    input_bits |= value << (loop_counter - 1 - i);
    this->clock_pin_->digital_write(false);
  }
  return input_bits;
}

void SN74HC165GPIOBinarySensor::process(const uint32_t data){
  const uint32_t idx = (1 << this->pin_);
  const bool value = data & idx;
  publish_state(value);
}

}  // namespace sn74hc165
}  // namespace esphome
