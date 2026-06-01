#ifdef USE_HOST
#include "sdl_encoder.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::sdl {

void SdlEncoder::setup() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "SDL parent not set");
    this->mark_failed();
    return;
  }

  // This encoder is driven by SDL mouse-wheel events rather than GPIO pins, so we deliberately do
  // not call RotaryEncoderSensor::setup() (which configures GPIO interrupts). Instead we initialize
  // the base state that loop() reads, and disable RTC persistence since there is no hardware encoder
  // whose value would need restoring.
  this->restore_mode_ = rotary_encoder::ROTARY_ENCODER_ALWAYS_ZERO;
  this->publish_initial_value_ = false;
  int32_t initial_value = clamp(0, this->store_.min_value, this->store_.max_value);
  this->store_.counter = initial_value;
  this->store_.last_read = initial_value;

  ESP_LOGCONFIG(TAG, "Setting up SDL Encoder (Mouse Wheel mode)");
  this->parent_->register_encoder(this);
}

void SdlEncoder::dump_config() {
  RotaryEncoderSensor::dump_config();
  ESP_LOGCONFIG(TAG, "  Mode: Mouse Wheel");
  ESP_LOGCONFIG(TAG, "  Wrap: %s", this->wrap_ ? "YES" : "NO");
}

void SdlEncoder::handle_wheel_event(int32_t wheel_delta) {
  // Mouse wheel up = positive delta = increment counter
  // Mouse wheel down = negative delta = decrement counter
  // High-resolution wheels and fast scrolling can report magnitudes greater than 1, so apply each
  // step individually rather than treating the whole event as a single tick.
  int32_t min_val = this->store_.min_value;
  int32_t max_val = this->store_.max_value;
  int32_t step = wheel_delta > 0 ? 1 : -1;

  while (wheel_delta != 0) {
    int32_t current_value = this->store_.counter;
    if (step > 0) {
      if (current_value >= max_val) {
        if (this->wrap_) {
          this->store_.counter = min_val;
        }
        // else stay at max
      } else {
        this->store_.counter = current_value + 1;
      }
    } else {
      if (current_value <= min_val) {
        if (this->wrap_) {
          this->store_.counter = max_val;
        }
        // else stay at min
      } else {
        this->store_.counter = current_value - 1;
      }
    }
    wheel_delta -= step;
  }

  // Trigger the loop to publish the updated value
  this->loop();
}

}  // namespace esphome::sdl
#endif
