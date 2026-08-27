#include "st7123_touchscreen.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::st7123 {

static const char *const TAG = "st7123.touchscreen";

void ST7123Touchscreen::setup() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(5);
    this->reset_pin_->digital_write(false);  // TP_RESX is active low, assert for at least tRSTW (2ms)
    delay(5);
    this->reset_pin_->digital_write(true);
    // The controller needs up to 20ms to initialize after reset before it can be accessed.
    this->setup_time_ = millis() + 30;
  }
}

void ST7123Touchscreen::update() {
  // check if setup is complete
  if (this->setup_time_ != 0) {
    if (this->setup_time_ > millis())
      return;

    uint8_t status;
    if (this->read_register16(ST7123_REG_STATUS, &status, 1) != i2c::ERROR_OK) {
      this->mark_failed(LOG_STR("Failed to read status register"));  // will stop updates
      return;
    }
    if ((status & 0x0F) == ST7123_STATUS_INIT) {
      ESP_LOGD(TAG, "Controller still initializing");
      return;
    }
    if (this->interrupt_pin_ != nullptr) {
      this->interrupt_pin_->setup();
      // INT is held high when idle and pulses low when touch data is ready.
      this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
    }

    ESP_LOGD(TAG, "Status is %X", status);

    uint8_t data;
    if (this->read_register16(ST7123_REG_MAX_TOUCHES, &data, 1) == i2c::ERROR_OK && data != 0 &&
        data <= ST7123_MAX_TOUCHES) {
      this->max_touches_ = data;
    }

    // If no calibration was supplied, read the native coordinate resolution from the controller.
    if (this->x_raw_max_ == this->x_raw_min_ || this->y_raw_max_ == this->y_raw_min_) {
      uint8_t res[4];
      if (this->read_register16(ST7123_REG_MAX_X, res, sizeof(res)) == i2c::ERROR_OK) {
        this->x_raw_max_ = encode_uint16(res[0] & ST7123_COORD_HIGH_MASK, res[1]);
        this->y_raw_max_ = encode_uint16(res[2] & ST7123_COORD_HIGH_MASK, res[3]);
        if (this->swap_x_y_)
          std::swap(this->x_raw_max_, this->y_raw_max_);
      } else {
        this->mark_failed(LOG_STR("Failed to read calibration"));
        return;
      }
      ESP_LOGD(TAG, "Read dimensions %d/%d", this->x_raw_max_, this->y_raw_max_);
    }
    this->setup_time_ = 0;  // flag setup complete
  }
  Touchscreen::update();
}

void ST7123Touchscreen::update_touches() {
  // Read the reporting table from the advanced touch info register through the last touch point.
  // Reading from this register also clears the INT pin so the controller can report the next frame.
  uint8_t data[(ST7123_REG_TOUCH_DATA - ST7123_REG_ADV_TOUCH_INFO) + ST7123_MAX_TOUCHES * ST7123_TOUCH_STRIDE];
  const size_t len = (ST7123_REG_TOUCH_DATA - ST7123_REG_ADV_TOUCH_INFO) + this->max_touches_ * ST7123_TOUCH_STRIDE;
  if (this->read_register16(ST7123_REG_ADV_TOUCH_INFO, data, len) != i2c::ERROR_OK) {
    this->skip_update_ = true;
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();

  const uint8_t *points = data + (ST7123_REG_TOUCH_DATA - ST7123_REG_ADV_TOUCH_INFO);
  for (uint8_t i = 0; i != this->max_touches_; i++) {
    const uint8_t *p = points + i * ST7123_TOUCH_STRIDE;
    if ((p[0] & ST7123_TOUCH_VALID) == 0)
      continue;
    uint16_t x = encode_uint16(p[0] & ST7123_COORD_HIGH_MASK, p[1]);
    uint16_t y = encode_uint16(p[2] & ST7123_COORD_HIGH_MASK, p[3]);
    uint8_t intensity = p[5];
    ESP_LOGV(TAG, "Touch %u: x=%u, y=%u, intensity=%u", i, x, y, intensity);
    this->add_raw_touch_position_(i, x, y, intensity);
  }
}

void ST7123Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ST7123 Touchscreen:\n"
                "  Max touches: %u\n"
                "  X Raw Min: %d, X Raw Max: %d\n"
                "  Y Raw Min: %d, Y Raw Max: %d",
                this->max_touches_, this->x_raw_min_, this->x_raw_max_, this->y_raw_min_, this->y_raw_max_);
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

}  // namespace esphome::st7123
