#include "cd74hc4067.h"
#include "esphome/core/log.h"
#include <Arduino.h>

namespace esphome {
namespace cd74hc4067 {

static const char *TAG = "cd74hc4067";

float CD74HC4067Component::get_setup_priority() const { return setup_priority::DATA; }

void CD74HC4067Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CD74HC4067...");

  this->pin_s0_->setup();
  this->pin_s1_->setup();
  this->pin_s2_->setup();
  this->pin_s3_->setup();

  // set other pin, so that activate_pin will really switch
  active_pin_ = 1;
  activate_pin(0);
}

void CD74HC4067Component::dump_config() {
  ESP_LOGCONFIG(TAG, "CD74HC4067 Multiplexer:");
  LOG_PIN("  S0 Pin: ", this->pin_s0_);
  LOG_PIN("  S1 Pin: ", this->pin_s1_);
  LOG_PIN("  S2 Pin: ", this->pin_s2_);
  LOG_PIN("  S3 Pin: ", this->pin_s3_);
}

void CD74HC4067Component::activate_pin(uint8_t pin) {
  if (active_pin_ != pin) {
    ESP_LOGD(TAG, "switch to input %d", pin);

    this->pin_s0_->digital_write(HIGH && bitRead(pin, 0));
    this->pin_s1_->digital_write(HIGH && bitRead(pin, 1));
    this->pin_s2_->digital_write(HIGH && bitRead(pin, 2));
    this->pin_s3_->digital_write(HIGH && bitRead(pin, 3));
    delay(10);
    active_pin_ = pin;
  }
}

CD74HC4067Sensor::CD74HC4067Sensor(CD74HC4067Component *parent, std::string name, uint8_t pin)
    : PollingComponent(1000), parent_(parent), pin_(pin) {
  this->set_name(name);
}
void CD74HC4067Sensor::setup() { LOG_SENSOR("", "Setting up CD74HC4067 Multiplexer '%s'...", this); }

void CD74HC4067Sensor::update() {
  float value_v = this->sample();
  this->publish_state(value_v);
}

float CD74HC4067Sensor::get_setup_priority() const { return parent_->get_setup_priority() - 1; }

float CD74HC4067Sensor::sample() {
  this->parent_->activate_pin(pin_);
  return source_->sample();
}

std::string CD74HC4067Sensor::unique_id() { return get_mac_address() + "-" + to_string(pin_); }

void CD74HC4067Sensor::dump_config() {
  LOG_SENSOR(TAG, "Multiplexer Sensor", this);
  ESP_LOGCONFIG(TAG, "CD74HC4067 Pin: %u", this->pin_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace cd74hc4067
}  // namespace esphome
