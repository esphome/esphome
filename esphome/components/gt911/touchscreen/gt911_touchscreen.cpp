#include "gt911_touchscreen.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gt911 {

static const char *const TAG = "gt911.touchscreen";

static const uint8_t GET_TOUCH_STATE[2] = {0x81, 0x4E};
static const uint8_t CLEAR_TOUCH_STATE[3] = {0x81, 0x4E, 0x00};
static const uint8_t GET_TOUCHES[2] = {0x81, 0x4F};
static const uint8_t GET_SWITCHES[2] = {0x80, 0x4D};
static const size_t MAX_TOUCHES = 5;  // max number of possible touches reported

#define ERROR_CHECK(err) \
  if ((err) != i2c::ERROR_OK) { \
    ESP_LOGE(TAG, "Failed to communicate!"); \
    this->status_set_warning(); \
    return; \
  }

void IRAM_ATTR HOT Store::gpio_intr(Store *store) { store->available = true; }

void GT911Touchscreen::setup() {
  i2c::ErrorCode err;
  ESP_LOGCONFIG(TAG, "Setting up GT911 Touchscreen...");
  // datasheet says NOT to use pullup/down on the int line.
  this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT);
  this->interrupt_pin_->setup();

  // check the configuration of the int line.
  uint8_t data;
  err = this->write(GET_SWITCHES, 2);
  if (err == i2c::ERROR_OK) {
    err = this->read(&data, 1);
    if (err == i2c::ERROR_OK) {
      ESP_LOGD(TAG, "Read from switches: 0x%02X", data);
      this->interrupt_pin_->attach_interrupt(Store::gpio_intr, &this->store_,
                                             (data & 1) ? gpio::INTERRUPT_FALLING_EDGE : gpio::INTERRUPT_RISING_EDGE);
    }
  }
  if (err != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to communicate!");
    this->mark_failed();
    return;
  }
  ESP_LOGCONFIG(TAG, "GT911 Touchscreen setup complete");
}

void GT911Touchscreen::loop() {
  i2c::ErrorCode err;
  touchscreen::TouchPoint tp;
  uint8_t touch_state = 0;
  uint8_t data[MAX_TOUCHES + 1][8];  // 8 bytes each for each point, plus extra space for the key byte

  if (!this->store_.available)
    return;
  this->store_.available = false;

  err = this->write(GET_TOUCH_STATE, sizeof(GET_TOUCH_STATE), false);
  ERROR_CHECK(err);
  err = this->read(&touch_state, 1);
  ERROR_CHECK(err);
  this->write(CLEAR_TOUCH_STATE, sizeof(CLEAR_TOUCH_STATE));

  if ((touch_state & 0x80) == 0)
    return;
  uint8_t num_of_touches = touch_state & 0x07;
  if (num_of_touches == 0)
    this->send_release_();
  if (num_of_touches > MAX_TOUCHES)  // should never happen
    return;

  err = this->write(GET_TOUCHES, sizeof(GET_TOUCHES), false);
  ERROR_CHECK(err);
  // num_of_touches is guaranteed to be 0..5. Also read the key data
  err = this->read(data[0], sizeof(data[0]) * num_of_touches + 1);
  ERROR_CHECK(err);

  for (uint8_t i = 0; i != num_of_touches; i++) {
    tp.id = data[i][0];
    uint16_t x = encode_uint16(data[i][2], data[i][1]);
    uint16_t y = encode_uint16(data[i][4], data[i][3]);

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
        tp.x = this->display_width_ - x;
        tp.y = this->display_height_ - y;
        break;
      case touchscreen::ROTATE_270_DEGREES:
        tp.x = this->display_height_ - y;
        tp.y = x;
        break;
    }
    this->defer([this, tp]() { this->send_touch_(tp); });
  }
  auto keys = data[num_of_touches][0];
  for (size_t i = 0; i != 4; i++) {
    for (auto *listener : this->button_listeners_)
      listener->update_button(i, (keys & (1 << i)) != 0);
  }
}

void GT911Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "GT911 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  ESP_LOGCONFIG(TAG, "  Rotation: %d", (int) this->rotation_);
}

}  // namespace gt911
}  // namespace esphome
