/**************************************************************************/
/*!
  Author: Gustavo Ambrozio
  Based on work by: Atsushi Sasaki (https://github.com/aselectroworks/Arduino-FT6336U)
*/
/**************************************************************************/

#include "ft63x6.h"
#include "esphome/core/log.h"

// Registers
// Reference: https://focuslcds.com/content/FT6236.pdf
namespace esphome {
namespace ft63x6 {
static const uint8_t FT6X36_ADDR_DEVICE_MODE = 0x00;

static const uint8_t FT63X6_ADDR_TD_STATUS = 0x02;
static const uint8_t FT63X6_ADDR_TOUCH1_STATE = 0x03;
static const uint8_t FT63X6_ADDR_TOUCH1_X = 0x03;
static const uint8_t FT63X6_ADDR_TOUCH1_ID = 0x05;
static const uint8_t FT63X6_ADDR_TOUCH1_Y = 0x05;
static const uint8_t FT63X6_ADDR_TOUCH1_WEIGHT = 0x07;
static const uint8_t FT63X6_ADDR_TOUCH1_MISC = 0x08;
static const uint8_t FT6X36_ADDR_THRESHHOLD = 0x80;
static const uint8_t FT6X36_ADDR_TOUCHRATE_ACTIVE = 0x88;
static const uint8_t FT63X6_ADDR_CHIP_ID = 0xA3;

static const char *const TAG = "FT63X6";

void FT63X6Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up FT63X6 Touchscreen...");
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_ANY_EDGE);
  }

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->hard_reset_();
  }

  // Get touch resolution
  if (this->x_raw_max_ == this->x_raw_min_) {
    this->x_raw_max_ = 320;
  }
  if (this->y_raw_max_ == this->y_raw_min_) {
    this->y_raw_max_ = 480;
  }
  uint8_t chip_id = this->read_byte_(FT63X6_ADDR_CHIP_ID);
  if (chip_id != 0) {
    ESP_LOGI(TAG, "FT6336U touch driver started chipid: %d", chip_id);
  } else {
    ESP_LOGE(TAG, "FT6336U touch driver failed to start");
  }
  this->write_byte(FT6X36_ADDR_DEVICE_MODE, 0x00);
  this->write_byte(FT6X36_ADDR_THRESHHOLD, this->threshold_);
  this->write_byte(FT6X36_ADDR_TOUCHRATE_ACTIVE, 0x0E);
}

void FT63X6Touchscreen::hard_reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
  }
}

void FT63X6Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "FT63X6 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void FT63X6Touchscreen::update_touches() {
  uint16_t touch_id, x, y;

  uint8_t touches = this->read_touch_number_();
  ESP_LOGV(TAG, "Touches found: %d", touches);
  if ((touches == 0x00) || (touches == 0xff)) {
    // ESP_LOGD(TAG, "No touches detected");
    return;
  }

  for (auto point = 0; point < touches; point++) {
    if (((this->read_touch_event_(point)) & 0x01) == 0) {  // checking event flag bit 6 if it is null
      touch_id = this->read_touch_id_(point);              // id1 = 0 or 1
      x = this->read_touch_x_(point);
      y = this->read_touch_y_(point);
      if ((x == 0) && (y == 0)) {
        ESP_LOGW(TAG, "Reporting a (0,0) touch on %d", touch_id);
      }
      this->add_raw_touch_position_(touch_id, x, y, this->read_touch_weight_(point));
    }
  }
}

uint8_t FT63X6Touchscreen::read_touch_number_() { return this->read_byte_(FT63X6_ADDR_TD_STATUS) & 0x0F; }
// Touch 1 functions
uint16_t FT63X6Touchscreen::read_touch_x_(uint8_t touch) {
  uint8_t read_buf[2];
  read_buf[0] = this->read_byte_(FT63X6_ADDR_TOUCH1_X + (touch * 6));
  read_buf[1] = this->read_byte_(FT63X6_ADDR_TOUCH1_X + 1 + (touch * 6));
  return ((read_buf[0] & 0x0f) << 8) | read_buf[1];
}
uint16_t FT63X6Touchscreen::read_touch_y_(uint8_t touch) {
  uint8_t read_buf[2];
  read_buf[0] = this->read_byte_(FT63X6_ADDR_TOUCH1_Y + (touch * 6));
  read_buf[1] = this->read_byte_(FT63X6_ADDR_TOUCH1_Y + 1 + (touch * 6));
  return ((read_buf[0] & 0x0f) << 8) | read_buf[1];
}
uint8_t FT63X6Touchscreen::read_touch_event_(uint8_t touch) {
  return this->read_byte_(FT63X6_ADDR_TOUCH1_X + (touch * 6)) >> 6;
}
uint8_t FT63X6Touchscreen::read_touch_id_(uint8_t touch) {
  return this->read_byte_(FT63X6_ADDR_TOUCH1_ID + (touch * 6)) >> 4;
}
uint8_t FT63X6Touchscreen::read_touch_weight_(uint8_t touch) {
  return this->read_byte_(FT63X6_ADDR_TOUCH1_WEIGHT + (touch * 6));
}
uint8_t FT63X6Touchscreen::read_touch_misc_(uint8_t touch) {
  return this->read_byte_(FT63X6_ADDR_TOUCH1_MISC + (touch * 6)) >> 4;
}

uint8_t FT63X6Touchscreen::read_byte_(uint8_t addr) {
  uint8_t byte = 0;
  this->read_byte(addr, &byte);
  return byte;
}

}  // namespace ft63x6
}  // namespace esphome
