#include "cd74hc4051.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cd74hc4051 {

static const char *const TAG = "cd74hc4051";

float CD74HC4051Component::get_setup_priority() const { return setup_priority::DATA; }

void CD74HC4051Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CD74HC4051...");

  this->pin_s0_->setup();
  this->pin_s1_->setup();
  this->pin_s2_->setup();

  // set other pin, so that activate_pin will really switch
  this->active_pin_ = 1;
  this->activate_pin(0);
}

void CD74HC4051Component::dump_config() {
  ESP_LOGCONFIG(TAG, "CD74HC4051 Multiplexer:");
  LOG_PIN("  S0 Pin: ", this->pin_s0_);
  LOG_PIN("  S1 Pin: ", this->pin_s1_);
  LOG_PIN("  S2 Pin: ", this->pin_s2_);
  ESP_LOGCONFIG(TAG, "switch delay: %d", this->switch_delay_);
}

void CD74HC4051Component::activate_pin(uint8_t pin) {
  if (this->active_pin_ != pin) {
    ESP_LOGD(TAG, "switch to input %d", pin);

    static int mux_channel[8][3] = {
        {0, 0, 0},  // channel 0
        {1, 0, 0},  // channel 1
        {0, 1, 0},  // channel 2
        {1, 1, 0},  // channel 3
        {0, 0, 1},  // channel 4
        {1, 0, 1},  // channel 5
        {0, 1, 1},  // channel 6
        {1, 1, 1}   // channel 7
    };
    this->pin_s0_->digital_write(mux_channel[pin][0]);
    this->pin_s1_->digital_write(mux_channel[pin][1]);
    this->pin_s2_->digital_write(mux_channel[pin][2]);
    // small delay is needed to let the multiplexer switch
    delay(this->switch_delay_);
    this->active_pin_ = pin;
  }
}

CD74HC4051Sensor::CD74HC4051Sensor(CD74HC4051Component *parent) : parent_(parent) {}

void CD74HC4051Sensor::update() {
  float value_v = this->sample();
  this->publish_state(value_v);
}

float CD74HC4051Sensor::get_setup_priority() const { return this->parent_->get_setup_priority() - 1.0f; }

float CD74HC4051Sensor::sample() {
  this->parent_->activate_pin(this->pin_);
  return this->source_->sample();
}

void CD74HC4051Sensor::dump_config() {
  LOG_SENSOR(TAG, "CD74HC4051 Sensor", this);
  ESP_LOGCONFIG(TAG, "  Pin: %u", this->pin_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace cd74hc4051
}  // namespace esphome
