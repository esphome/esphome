#ifdef USE_HOST
#include "sdl_encoder.h"
#include "esphome/core/log.h"

namespace esphome::sdl {

void SdlEncoder::setup() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "SDL parent not set");
    this->mark_failed();
    return;
  }

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
  int32_t current_value = this->store_.counter;
  int32_t min_val = this->store_.min_value;
  int32_t max_val = this->store_.max_value;

  if (wheel_delta > 0) {
    if (current_value >= max_val) {
      if (this->wrap_) {
        this->store_.counter = min_val;
      }
      // else stay at max
    } else {
      this->store_.counter += 1;
    }
  } else if (wheel_delta < 0) {
    if (current_value <= min_val) {
      if (this->wrap_) {
        this->store_.counter = max_val;
      }
      // else stay at min
    } else {
      this->store_.counter -= 1;
    }
  }

  // Trigger the loop to publish the updated value
  this->loop();
}

}  // namespace esphome::sdl
#endif
