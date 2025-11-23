#include "st7123_touchscreen.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace st7123 {

static const char *const TAG = "st7123.touchscreen";

// Registers
static const uint16_t REG_GET_TOUCH_INFO = 0x0010;
static const uint16_t REG_GET_TOUCH = 0x0014;
static const uint8_t MAX_TOUCHES = 10;

struct touch_data_t {
  uint8_t x_h : 6;
  uint8_t reserved_6 : 1;
  uint8_t valid : 1;
  uint8_t x_l;
  uint8_t y_h;
  uint8_t y_l;
  uint8_t area;
  uint8_t intensity;
  uint8_t reserved_49_55;
};

struct adv_info_t {
  uint8_t reserved_0_1 : 2;
  uint8_t with_prox : 1;
  uint8_t with_coord : 1;
  uint8_t prox_status : 3;
  uint8_t rst_chip : 1;
};

void ST7123Touchscreen::setup() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(5);
    this->reset_pin_->digital_write(false);
    delay(5);
    this->reset_pin_->digital_write(true);
    this->set_timeout(25, [this] { this->setup_internal_(); });
    return;
  }
  this->setup_internal_();
}

void ST7123Touchscreen::setup_internal_() {
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_,
                            gpio::INTERRUPT_RISING_EDGE);  // INTERRUPT_RISING_EDGE / INTERRUPT_FALLING_EDGE?
  }

  this->setup_done_ = true;
}

void ST7123Touchscreen::update_touches() {
  this->skip_update_ = true;  // skip send touch events by default, set to false after successful error checks
  if (!this->setup_done_) {
    return;
  }
  i2c::ErrorCode err;
  touch_data_t touch_data[MAX_TOUCHES];
  adv_info_t adv_info;

  err = this->read_register16(REG_GET_TOUCH_INFO, (uint8_t *) &adv_info, 1);
  if (err == i2c::ERROR_OK) {
    if (adv_info.with_coord) {
      err = this->read_register16(REG_GET_TOUCH, (uint8_t *) &touch_data[0], sizeof(touch_data_t) * MAX_TOUCHES);
      if (err == i2c::ERROR_OK) {
        for (size_t i = 0; i < MAX_TOUCHES; i++) {
          if (!touch_data[i].valid) {
            continue;
          }
          uint16_t xpos = encode_uint16(touch_data[i].x_h, touch_data[i].x_l);
          uint16_t ypos = encode_uint16(touch_data[i].y_h, touch_data[i].y_l);
          uint16_t id = touch_data[i].area;
          this->add_raw_touch_position_(id, xpos, ypos);
        }
      }
    }
  }

  this->skip_update_ = false;  // All error checks passed, send touch events

  return;
}

void ST7123Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "ST7123 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

}  // namespace st7123
}  // namespace esphome
