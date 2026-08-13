#include "icnt86.h"
#include "esphome/core/log.h"

namespace esphome::icnt86 {

static const char *const TAG = "icnt86";

void ICNT86Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up icnt86 Touchscreen...");

  // Register interrupt pin
  this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->interrupt_pin_->setup();
  this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);

  // Perform reset if necessary
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_();
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
  uint8_t buf[100] = {0};
  char mask[1] = {0x00};

  this->i2c_read_byte_(0x1001, buf, 1);
  uint8_t touch_count = buf[0];

  if (touch_count == 0x00 || (touch_count > 5 || touch_count < 1)) {  // No new touch
    return;
  } else {
    this->i2c_read_byte_(0x1002, buf, touch_count * 7);
    this->i2c_write_byte_(0x1001, mask, 1);
    ESP_LOGD(TAG, "Touch count: %d", touch_count);

    for (uint8_t i = 0; i < touch_count; i++) {
      uint16_t x = ((uint16_t) buf[2 + 7 * i] << 8) + buf[1 + 7 * i];
      uint16_t y = ((uint16_t) buf[4 + 7 * i] << 8) + buf[3 + 7 * i];
      uint8_t p = buf[5 + 7 * i];
      uint8_t touch_evenid = buf[6 + 7 * i];

      if (i == 0) {
        if (touch_count == 1 && x == x_old_ && y == y_old_ && p == 0 && p_old_zero_) {
          return;  // Skip this touch because previous was also zero
        }
        x_old_ = x;
        y_old_ = y;
        p_old_zero_ = (p == 0);
      } else if (p > 0) {
        p_old_zero_ = false;
      }
      this->add_raw_touch_position_(touch_evenid, x, y, p);
    }
  }
}

void ICNT86Touchscreen::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
    delay(10);
  }
}

void ICNT86Touchscreen::i2c_write_byte_(uint16_t reg, const char *data, uint8_t len) {
  char wbuf[50] = {static_cast<char>(reg >> 8 & 0xff), static_cast<char>(reg & 0xff)};
  for (uint8_t i = 0; i < len; i++) {
    wbuf[i + 2] = data[i];
  }
  this->write((const uint8_t *) wbuf, len + 2);
}

void ICNT86Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "icnt86 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

void ICNT86Touchscreen::i2c_read_byte_(uint16_t reg, uint8_t *data, uint8_t len) {
  this->i2c_write_byte_(reg, nullptr, 0);
  this->read(data, len);
}

}  // namespace esphome::icnt86
