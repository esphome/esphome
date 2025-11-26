#include "st7123_touchscreen.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace st7123 {

static const char *const TAG = "st7123.touchscreen";

// Registers
static const uint16_t REG_GET_TOUCH_INFO = 0x0010;
static const uint16_t REG_GET_TOUCH = 0x0014;
static const uint16_t REG_GET_KEYS = 0x0013;
static const uint16_t REG_GET_MAX_COORD = 0x0005;
static const uint8_t MAX_TOUCHES = 10;
static const uint8_t MAX_BUTTONS = 6;

struct TouchData {
  uint8_t x_h : 6;
  uint8_t reserved_6 : 1;
  uint8_t valid : 1;
  uint8_t x_l;
  uint8_t y_h : 6;
  uint8_t reserved_6_7 : 2;
  uint8_t y_l;
  uint8_t area;
  uint8_t intensity;
  uint8_t reserved;
};

struct AdvInfo {
  uint8_t reserved : 2;
  uint8_t with_prox : 1;
  uint8_t with_coord : 1;
  uint8_t prox_status : 3;
  uint8_t rst_chip : 1;
};

struct MaxCoordInfo {
  uint8_t max_x_h : 6;
  uint8_t reserved_x_7_8 : 2;
  uint8_t max_x_l;
  uint8_t max_y_h : 6;
  uint8_t reserved_y_7_8 : 2;
  uint8_t max_y_l;
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

void ST7123Touchscreen::setup_lazy_() {
  MaxCoordInfo max_coord_info;
  ESP_LOGD(TAG, "Reading max touch coordinates");
  // no calibration? Attempt to read the max values from the touchscreen.
  i2c::ErrorCode err = this->read_register16(REG_GET_MAX_COORD, (uint8_t *) &max_coord_info, sizeof(MaxCoordInfo));
  if (err == i2c::ERROR_OK) {
    this->x_raw_max_ = encode_uint16(max_coord_info.max_x_h, max_coord_info.max_x_l);
    this->y_raw_max_ = encode_uint16(max_coord_info.max_y_h, max_coord_info.max_y_l);
    ESP_LOGD(TAG, "Max Coord: %d %d", x_raw_max_, y_raw_max_);
    if (this->swap_x_y_)
      std::swap(this->x_raw_max_, this->y_raw_max_);
  } else {
    this->mark_failed(LOG_STR("Calibration error"));
  }
}

void ST7123Touchscreen::update_touches() {
  this->skip_update_ = true;  // skip send touch events by default, set to false after successful error checks
  if (!this->setup_done_) {
    return;
  }
  if (x_raw_max_ == 0 || y_raw_max_ == 0) {
    setup_lazy_();
  }
  i2c::ErrorCode err;
  TouchData touch_data[MAX_TOUCHES];
  AdvInfo adv_info;

  err = this->read_register16(REG_GET_TOUCH_INFO, (uint8_t *) &adv_info, 1);
  if (err == i2c::ERROR_OK) {
    if (adv_info.with_coord) {
      err = this->read_register16(REG_GET_TOUCH, (uint8_t *) &touch_data[0], sizeof(TouchData) * MAX_TOUCHES);
      if (err == i2c::ERROR_OK) {
        for (auto &i : touch_data) {
          if (!i.valid) {
            continue;
          }
          uint16_t xpos = encode_uint16(i.x_h, i.x_l);
          uint16_t ypos = encode_uint16(i.y_h, i.y_l);
          uint16_t id = i.area;
          this->add_raw_touch_position_(id, xpos, ypos);
        }
      }
    }
  }

  uint8_t keys;
  err = this->read_register16(REG_GET_KEYS, &keys, 1);
  if (err == i2c::ERROR_OK) {
    if (keys != this->button_state_) {
      this->button_state_ = keys;
      for (size_t i = 0; i != MAX_BUTTONS; i++) {
        for (auto *listener : this->button_listeners_)
          listener->update_button(i, (keys & (1 << i)) != 0);
      }
    }
  }

  this->skip_update_ = false;  // All error checks passed, send touch events
}

void ST7123Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "ST7123 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

}  // namespace st7123
}  // namespace esphome
