#include "chsc6x_touchscreen.h"

namespace esphome::chsc6x {

void CHSC6XTouchscreen::setup() {
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
}

void CHSC6XTouchscreen::update_touches() {
  if (this->model_ == CHSC6X_MODEL_CHSC6540) {
    this->update_touches_chsc6540_();
    return;
  }

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

void CHSC6XTouchscreen::update_touches_chsc6540_() {
  uint8_t data[CHSC6540_REG_STATUS_LEN];
  if (!this->read_bytes(CHSC6X_REG_STATUS, data, sizeof(data))) {
    return;
  }

  const uint8_t num_of_touches = data[CHSC6540_REG_STATUS_TOUCH];
  // 0 is a genuine lift event; 0xFF turns up in unpopulated point slots.
  //
  // No upper bound is imposed on the count deliberately. The panel this was
  // captured on reports at most 2, but how many points other parts in the
  // family report is unknown, and a hard ceiling would silently discard valid
  // frames on untested hardware. The count is only used here as "at least one
  // point is present" -- point 0 is the only one reported either way -- and the
  // all-0xFF coordinate check below rejects the one corrupt frame actually
  // observed.
  if (num_of_touches == 0 || num_of_touches == 0xFF) {
    return;
  }

  // Occasional glitch frame: a plausible touch count with every coordinate byte
  // set to 0xFF, seen roughly once per drag. It would decode to (4095, 4095).
  if ((data[CHSC6540_REG_STATUS_X_HI] & data[CHSC6540_REG_STATUS_X_LO] & data[CHSC6540_REG_STATUS_Y_HI] &
       data[CHSC6540_REG_STATUS_Y_LO]) == 0xFF) {
    return;
  }

  const uint16_t x = (static_cast<uint16_t>(data[CHSC6540_REG_STATUS_X_HI] & CHSC6540_COORD_HI_MASK) << 8) |
                     data[CHSC6540_REG_STATUS_X_LO];
  const uint16_t y = (static_cast<uint16_t>(data[CHSC6540_REG_STATUS_Y_HI] & CHSC6540_COORD_HI_MASK) << 8) |
                     data[CHSC6540_REG_STATUS_Y_LO];

  this->add_raw_touch_position_(0, x, y);
}

void CHSC6XTouchscreen::dump_config() {
  ESP_LOGCONFIG(TAG,
                "CHSC6X Touchscreen:\n"
                "  Model: %s\n"
                "  Touch timeout: %d\n"
                "  x_raw_max_: %d\n"
                "  y_raw_max_: %d",
                this->model_ == CHSC6X_MODEL_CHSC6540 ? "CHSC6540" : "CHSC6X", this->touch_timeout_, this->x_raw_max_,
                this->y_raw_max_);
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
}

}  // namespace esphome::chsc6x
