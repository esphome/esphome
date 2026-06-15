#include "st7123_touchscreen.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::st7123 {

static const char *const TAG = "st7123.touchscreen";

bool ST7123Touchscreen::read_register_(uint16_t reg, uint8_t *data, size_t len) const {
  const uint8_t addr[2] = {static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg & 0xFF)};
  if (this->write(addr, sizeof(addr)) != i2c::ERROR_OK)
    return false;
  return this->read(data, len) == i2c::ERROR_OK;
}

void ST7123Touchscreen::setup() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(5);
    this->reset_pin_->digital_write(false);  // TP_RESX is active low, assert for at least tRSTW (2ms)
    delay(5);
    this->reset_pin_->digital_write(true);
    // The controller needs up to 20ms to initialize after reset before it can be accessed.
    this->set_timeout(20, [this] { this->continue_setup_(); });
  } else {
    this->continue_setup_();
  }
}

void ST7123Touchscreen::continue_setup_() {
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    // INT is held high when idle and pulses low when touch data is ready.
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }

  uint8_t status;
  if (!this->read_register_(ST7123_REG_STATUS, &status, 1)) {
    this->status_set_error(LOG_STR("Failed to read status register"));
    this->mark_failed();
    return;
  }
  if ((status & 0x0F) == ST7123_STATUS_INIT) {
    ESP_LOGW(TAG, "Controller still initializing (status 0x%02X)", status);
  }

  uint8_t data;
  if (this->read_register_(ST7123_REG_MAX_TOUCHES, &data, 1) && data != 0 && data <= ST7123_MAX_TOUCHES) {
    this->max_touches_ = data;
  }

  // If no calibration was supplied, read the native coordinate resolution from the controller.
  if (this->x_raw_max_ == this->x_raw_min_ || this->y_raw_max_ == this->y_raw_min_) {
    uint8_t res[4];
    if (this->read_register_(ST7123_REG_MAX_X, res, sizeof(res))) {
      this->x_raw_max_ = encode_uint16(res[0] & ST7123_COORD_HIGH_MASK, res[1]);
      this->y_raw_max_ = encode_uint16(res[2] & ST7123_COORD_HIGH_MASK, res[3]);
      if (this->swap_x_y_)
        std::swap(this->x_raw_max_, this->y_raw_max_);
    }
  }
}

void ST7123Touchscreen::update_touches() {
  // Read the reporting table from the advanced touch info register through the last touch point.
  // Reading from this register also clears the INT pin so the controller can report the next frame.
  uint8_t data[(ST7123_REG_TOUCH_DATA - ST7123_REG_ADV_TOUCH_INFO) + ST7123_MAX_TOUCHES * ST7123_TOUCH_STRIDE];
  const size_t len = (ST7123_REG_TOUCH_DATA - ST7123_REG_ADV_TOUCH_INFO) + this->max_touches_ * ST7123_TOUCH_STRIDE;
  if (!this->read_register_(ST7123_REG_ADV_TOUCH_INFO, data, len)) {
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
