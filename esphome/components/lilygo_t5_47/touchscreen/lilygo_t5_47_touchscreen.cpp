#include "lilygo_t5_47_touchscreen.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lilygo_t5_47 {

static const char *const TAG = "lilygo_t5_47.touchscreen";

static const uint8_t POWER_REGISTER = 0xD6;
static const uint8_t TOUCH_REGISTER = 0xD0;

static const uint8_t WAKEUP_CMD[1] = {0x06};
static const uint8_t READ_FLAGS[1] = {0x00};
static const uint8_t CLEAR_FLAGS[2] = {0x00, 0xAB};
static const uint8_t READ_TOUCH[1] = {0x07};

#define ERROR_CHECK(err) \
  if ((err) != i2c::ERROR_OK) { \
    ESP_LOGE(TAG, "Failed to communicate!"); \
    this->status_set_warning(); \
    return; \
  }

void LilygoT547Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Lilygo T5 4.7 Touchscreen...");
  this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->interrupt_pin_->setup();

  this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);

  if (this->write(nullptr, 0) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to communicate!");
    this->interrupt_pin_->detach_interrupt();
    this->mark_failed();
    return;
  }

  this->write_register(POWER_REGISTER, WAKEUP_CMD, 1);
  if (this->display_ != nullptr) {
    if (this->x_raw_max_ == this->x_raw_min_) {
      this->x_raw_max_ = this->display_->get_native_width();
    }
    if (this->y_raw_max_ == this->y_raw_min_) {
      this->x_raw_max_ = this->display_->get_native_height();
    }
  }
}

void LilygoT547Touchscreen::update_touches() {
  uint8_t point = 0;
  uint8_t buffer[40] = {0};

  i2c::ErrorCode err;
  err = this->write_register(TOUCH_REGISTER, READ_FLAGS, 1);
  ERROR_CHECK(err);

  err = this->read(buffer, 7);
  ERROR_CHECK(err);

  if (buffer[0] == 0xAB) {
    this->write_register(TOUCH_REGISTER, CLEAR_FLAGS, 2);
    return;
  }

  point = buffer[5] & 0xF;

  if (point == 1) {
    err = this->write_register(TOUCH_REGISTER, READ_TOUCH, 1);
    ERROR_CHECK(err);
    err = this->read(&buffer[5], 2);
    ERROR_CHECK(err);

  } else if (point > 1) {
    err = this->write_register(TOUCH_REGISTER, READ_TOUCH, 1);
    ERROR_CHECK(err);
    err = this->read(&buffer[5], 5 * (point - 1) + 3);
    ERROR_CHECK(err);
  }

  this->write_register(TOUCH_REGISTER, CLEAR_FLAGS, 2);

  if (point == 0)
    point = 1;

  uint16_t id, x_raw, y_raw;
  for (uint8_t i = 0; i < point; i++) {
    id = (buffer[i * 5] >> 4) & 0x0F;
    y_raw = (uint16_t) ((buffer[i * 5 + 1] << 4) | ((buffer[i * 5 + 3] >> 4) & 0x0F));
    x_raw = (uint16_t) ((buffer[i * 5 + 2] << 4) | (buffer[i * 5 + 3] & 0x0F));
    this->add_raw_touch_position_(id, x_raw, y_raw);
  }

  this->status_clear_warning();
}

void LilygoT547Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "Lilygo T5 47 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
}

}  // namespace lilygo_t5_47
}  // namespace esphome
