#include "icnt86.h"
#include "esphome/core/log.h"

namespace esphome::icnt86 {

static const char *const TAG = "icnt86";
static const uint16_t REG_TOUCH_NUM = 0x1001;
static const uint16_t REG_POINT1 = 0x1002;
static const uint8_t MAX_TOUCHES = 5;
static const uint8_t POINT_SIZE = 7;

void ICNT86Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up icnt86 Touchscreen...");

  // Register interrupt pin
  this->interrupt_pin_->setup();
  this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);

  // Perform reset if necessary
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    if (this->reset_pin_ != nullptr) {
      this->reset_pin_->digital_write(false);
      delay(10);
      this->reset_pin_->digital_write(true);
    }
  }

  // Swap intentional: this touch chip's raw axes are transposed relative to the display's native orientation.
  if (this->x_raw_max_ == this->x_raw_min_) {
    this->x_raw_max_ = this->display_->get_native_height();
  }
  if (this->y_raw_max_ == this->y_raw_min_) {
    this->y_raw_max_ = this->display_->get_native_width();
  }
}

void ICNT86Touchscreen::update_touches() {
  uint8_t buf[MAX_TOUCHES * POINT_SIZE] = {0};
  uint8_t mask[1] = {0x00};

  if (this->read_register16(REG_TOUCH_NUM, buf, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    this->skip_update_ = true;
    ESP_LOGW(TAG, "Failed to read touch count");
    return;
  }
  uint8_t touch_count = buf[0];

  if (touch_count == 0x00 || (touch_count > MAX_TOUCHES || touch_count < 1)) {  // No new touch
    return;
  }
  if (this->read_register16(REG_POINT1, buf, touch_count * POINT_SIZE) != i2c::ERROR_OK) {
    this->status_set_warning();
    this->skip_update_ = true;
    ESP_LOGW(TAG, "Failed to read touch points");
    return;
  }
  this->write_register16(REG_TOUCH_NUM, mask, 1);
  ESP_LOGD(TAG, "Touch count: %d", touch_count);

  for (uint8_t i = 0; i < touch_count; i++) {
    uint16_t x = ((uint16_t) buf[2 + 7 * i] << 8) + buf[1 + 7 * i];
    uint16_t y = ((uint16_t) buf[4 + 7 * i] << 8) + buf[3 + 7 * i];
    uint8_t p = buf[5 + 7 * i];
    uint8_t touch_id = buf[6 + 7 * i];

    if (i == 0) {
      if (touch_count == 1 && x == this->x_old_ && y == this->y_old_ && p == 0 && this->p_old_zero_) {
        return;  // Skip this touch because previous was also zero
      }
      this->x_old_ = x;
      this->y_old_ = y;
      this->p_old_zero_ = (p == 0);
    } else if (p > 0) {
      this->p_old_zero_ = false;
    }
    this->add_raw_touch_position_(touch_id, x, y, p);
  }
}

void ICNT86Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "icnt86 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

}  // namespace esphome::icnt86
