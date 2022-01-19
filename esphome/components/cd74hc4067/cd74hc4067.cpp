#include "cd74hc4067.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cd74hc4067 {

static const char *const TAG = "cd74hc4067";

float CD74HC4067Component::get_setup_priority() const { return setup_priority::DATA; }

void CD74HC4067Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CD74HC4067...");

  this->pin_s0_->setup();
  this->pin_s1_->setup();
  this->pin_s2_->setup();
  this->pin_s3_->setup();

  // set other pin, so that activate_pin will really switch
  this->active_pin_ = 1;
  this->activate_pin(0);
}

void CD74HC4067Component::dump_config() {
  ESP_LOGCONFIG(TAG, "CD74HC4067 Multiplexer:");
  LOG_PIN("  S0 Pin: ", this->pin_s0_);
  LOG_PIN("  S1 Pin: ", this->pin_s1_);
  LOG_PIN("  S2 Pin: ", this->pin_s2_);
  LOG_PIN("  S3 Pin: ", this->pin_s3_);
  ESP_LOGCONFIG(TAG, "switch delay: %d", this->switch_delay_);
}

void CD74HC4067Component::activate_pin(uint8_t pin) {
  if (this->active_pin_ != pin) {
    ESP_LOGD(TAG, "switch to input %d", pin);

    static int mux_channel[16][4] = {
        {0, 0, 0, 0},  // channel 0
        {1, 0, 0, 0},  // channel 1
        {0, 1, 0, 0},  // channel 2
        {1, 1, 0, 0},  // channel 3
        {0, 0, 1, 0},  // channel 4
        {1, 0, 1, 0},  // channel 5
        {0, 1, 1, 0},  // channel 6
        {1, 1, 1, 0},  // channel 7
        {0, 0, 0, 1},  // channel 8
        {1, 0, 0, 1},  // channel 9
        {0, 1, 0, 1},  // channel 10
        {1, 1, 0, 1},  // channel 11
        {0, 0, 1, 1},  // channel 12
        {1, 0, 1, 1},  // channel 13
        {0, 1, 1, 1},  // channel 14
        {1, 1, 1, 1}   // channel 15
    };
    this->pin_s0_->digital_write(mux_channel[pin][0]);
    this->pin_s1_->digital_write(mux_channel[pin][1]);
    this->pin_s2_->digital_write(mux_channel[pin][2]);
    this->pin_s3_->digital_write(mux_channel[pin][3]);
    // small delay is needed to let the multiplexer switch
    delay(this->switch_delay_);
    this->active_pin_ = pin;
  }
}

CD74HC4067Sensor::CD74HC4067Sensor(CD74HC4067Component *parent) : parent_(parent) {}

void CD74HC4067Sensor::update() {
  float value_v = this->sample();
  this->publish_state(value_v);
}

float CD74HC4067Sensor::get_setup_priority() const { return this->parent_->get_setup_priority() - 1.0f; }

float CD74HC4067Sensor::sample() {
  this->parent_->activate_pin(this->pin_);
  return this->source_->sample();
}

void CD74HC4067Sensor::dump_config() {
  LOG_SENSOR(TAG, "CD74HC4067 Sensor", this);
  ESP_LOGCONFIG(TAG, "  Pin: %u", this->pin_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace cd74hc4067
}  // namespace esphome
