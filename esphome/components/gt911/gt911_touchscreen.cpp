#include "gt911_touchscreen.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gt911 {

static const char *const TAG = "gt911.touchscreen";

static const uint8_t GET_TOUCH_STATE[2] = {0x81, 0x4E};
static const uint8_t CLEAR_TOUCH_STATE[3] = {0x81, 0x4E, 0x00};
static const uint8_t GET_TOUCHES[2] = {0x81, 0x4F};

#define ERROR_CHECK(err) \
  if ((err) != i2c::ERROR_OK) { \
    ESP_LOGE(TAG, "Failed to communicate!"); \
    this->status_set_warning(); \
    return; \
  }

void IRAM_ATTR HOT Store::gpio_intr(Store *store) {
  store->available = true;
}

void GT911Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up GT911 Touchscreen...");
  this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->interrupt_pin_->setup();

  if (this->write(nullptr, 0) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to communicate!");
    this->mark_failed();
    return;
  }

  this->interrupt_pin_->attach_interrupt(Store::gpio_intr, &this->store_, gpio::INTERRUPT_FALLING_EDGE);
}

void GT911Touchscreen::loop() {
  if (!this->store_.available) {
    for (auto *listener : this->touch_listeners_)
      listener->release();
    return;
  }

  this->store_.available = false;

  uint8_t touch_state = 0;

  i2c::ErrorCode err;
  err = this->write(GET_TOUCH_STATE, sizeof(GET_TOUCH_STATE), false);
  ERROR_CHECK(err);
  err = this->read(&touch_state, 1);
  ERROR_CHECK(err);

  bool buffer_status = touch_state & 0x80;
  uint8_t num_of_touches = touch_state & 0x07;
  if (!buffer_status || num_of_touches == 0) {
    for (auto *listener : this->touch_listeners_)
      listener->release();
    if (buffer_status) {
      err = this->write(CLEAR_TOUCH_STATE, sizeof(CLEAR_TOUCH_STATE));
      ERROR_CHECK(err);
    }
    return;
  }

  uint8_t data[num_of_touches << 3];
  err = this->write(GET_TOUCHES, sizeof(GET_TOUCHES), false);
  ERROR_CHECK(err);
  err = this->read(data, sizeof(data));
  ERROR_CHECK(err);

  for (uint8_t i = 0; i < num_of_touches; i++) {
    uint8_t *d = data + (i << 3);

    touchscreen::TouchPoint tp;

    tp.id = d[0];

    uint16_t x = encode_uint16(d[2], d[1]);
    uint16_t y = encode_uint16(d[4], d[3]);

    switch (this->rotation_) {
      case touchscreen::ROTATE_0_DEGREES:
        tp.x = x;
        tp.y = y;
        break;
      case touchscreen::ROTATE_90_DEGREES:
        tp.x = y;
        tp.y = this->display_width_ - x;
        break;
      case touchscreen::ROTATE_180_DEGREES:
        tp.x = this->display_width_ - y;
        tp.y = this->display_height_ - x;
        break;
      case touchscreen::ROTATE_270_DEGREES:
        tp.x = this->display_height_ - y;
        tp.y = x;
        break;
    }
    this->defer([this, tp]() { this->send_touch_(tp); });
  }

  err = this->write(CLEAR_TOUCH_STATE, sizeof(CLEAR_TOUCH_STATE));
  ERROR_CHECK(err);
}

void GT911Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "GT911 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
}

}  // namespace gt911
}  // namespace esphome
