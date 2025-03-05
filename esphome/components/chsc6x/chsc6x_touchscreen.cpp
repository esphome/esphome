#include "chsc6x_touchscreen.h"

namespace esphome {
namespace chsc6x {

void CHSC6XTouchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CHSC6X Touchscreen...");
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }
  if (this->x_raw_max_ == this->x_raw_min_) {
    this->x_raw_max_ = this->display_->get_native_width();
  }
  if (this->y_raw_max_ == this->y_raw_min_) {
    this->y_raw_max_ = this->display_->get_native_height();
  }

  ESP_LOGCONFIG(TAG, "CHSC6X Touchscreen setup complete");
}

void CHSC6XTouchscreen::update_touches() {
  uint8_t data[CHSC6X_REG_STATUS_LEN];
  if (!this->read_bytes(CHSC6X_REG_STATUS, data, sizeof(data))) {
    return;
  }

  uint8_t num_of_touches = data[CHSC6X_REG_STATUS_TOUCH];

  if (num_of_touches == 1) {
    uint16_t x = data[CHSC6X_REG_STATUS_X_COR];
    uint16_t y = data[CHSC6X_REG_STATUS_Y_COR];
    this->add_raw_touch_position_(0, x, y);
  }
}

void CHSC6XTouchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "CHSC6X Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  ESP_LOGCONFIG(TAG, "  Touch timeout: %d", this->touch_timeout_);
  ESP_LOGCONFIG(TAG, "  x_raw_max_: %d", this->x_raw_max_);
  ESP_LOGCONFIG(TAG, "  y_raw_max_: %d", this->y_raw_max_);
}

}  // namespace chsc6x
}  // namespace esphome
