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

void Store::gpio_intr(Store *store) { store->touch = true; }

void LilygoT547Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Lilygo T5 4.7 Touchscreen...");
  this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->interrupt_pin_->setup();

  this->store_.pin = this->interrupt_pin_->to_isr();
  this->interrupt_pin_->attach_interrupt(Store::gpio_intr, &this->store_, gpio::INTERRUPT_FALLING_EDGE);

  if (this->write(nullptr, 0) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to communicate!");
    this->interrupt_pin_->detach_interrupt();
    this->mark_failed();
    return;
  }

  this->write_register(POWER_REGISTER, WAKEUP_CMD, 1);
}

void LilygoT547Touchscreen::loop() {
  if (!this->store_.touch) {
    for (auto *listener : this->touch_listeners_)
      listener->release();
    return;
  }
  this->store_.touch = false;

  uint8_t point = 0;
  uint8_t buffer[40] = {0};
  uint32_t sum_l = 0, sum_h = 0;

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

  if (point == 0) {
    for (auto *listener : this->touch_listeners_)
      listener->release();
    return;
  } else if (point == 1) {
    err = this->write_register(TOUCH_REGISTER, READ_TOUCH, 1);
    ERROR_CHECK(err);
    err = this->read(&buffer[5], 2);
    ERROR_CHECK(err);

    sum_l = buffer[5] << 8 | buffer[6];
  } else if (point > 1) {
    err = this->write_register(TOUCH_REGISTER, READ_TOUCH, 1);
    ERROR_CHECK(err);
    err = this->read(&buffer[5], 5 * (point - 1) + 3);
    ERROR_CHECK(err);

    sum_l = buffer[5 * point + 1] << 8 | buffer[5 * point + 2];
  }

  this->write_register(TOUCH_REGISTER, CLEAR_FLAGS, 2);

  for (int i = 0; i < 5 * point; i++)
    sum_h += buffer[i];

  if (sum_l != sum_h)
    point = 0;

  if (point) {
    uint8_t offset;
    for (int i = 0; i < point; i++) {
      if (i == 0) {
        offset = 0;
      } else {
        offset = 4;
      }

      TouchPoint tp;

      tp.id = (buffer[i * 5 + offset] >> 4) & 0x0F;
      tp.state = buffer[i * 5 + offset] & 0x0F;
      if (tp.state == 0x06)
        tp.state = 0x07;

      uint16_t y = (uint16_t) ((buffer[i * 5 + 1 + offset] << 4) | ((buffer[i * 5 + 3 + offset] >> 4) & 0x0F));
      uint16_t x = (uint16_t) ((buffer[i * 5 + 2 + offset] << 4) | (buffer[i * 5 + 3 + offset] & 0x0F));

      switch (this->rotation_) {
        case ROTATE_0_DEGREES:
          tp.y = this->display_height_ - y;
          tp.x = x;
          break;
        case ROTATE_90_DEGREES:
          tp.x = this->display_height_ - y;
          tp.y = this->display_width_ - x;
          break;
        case ROTATE_180_DEGREES:
          tp.y = y;
          tp.x = this->display_width_ - x;
          break;
        case ROTATE_270_DEGREES:
          tp.x = y;
          tp.y = x;
          break;
      }

      this->defer([this, tp]() { this->send_touch_(tp); });
    }
  } else {
    TouchPoint tp;
    tp.id = (buffer[0] >> 4) & 0x0F;
    tp.state = 0x06;

    uint16_t y = (uint16_t) ((buffer[0 * 5 + 1] << 4) | ((buffer[0 * 5 + 3] >> 4) & 0x0F));
    uint16_t x = (uint16_t) ((buffer[0 * 5 + 2] << 4) | (buffer[0 * 5 + 3] & 0x0F));

    switch (this->rotation_) {
      case ROTATE_0_DEGREES:
        tp.y = this->display_height_ - y;
        tp.x = x;
        break;
      case ROTATE_90_DEGREES:
        tp.x = this->display_height_ - y;
        tp.y = this->display_width_ - x;
        break;
      case ROTATE_180_DEGREES:
        tp.y = y;
        tp.x = this->display_width_ - x;
        break;
      case ROTATE_270_DEGREES:
        tp.x = y;
        tp.y = x;
        break;
    }

    this->defer([this, tp]() { this->send_touch_(tp); });
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
