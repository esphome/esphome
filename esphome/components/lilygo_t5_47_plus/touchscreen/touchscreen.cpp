#include "touchscreen.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <Arduino.h>

namespace esphome {
namespace lilygo_t5_47_plus {

static const char *const TAG = "lilygo_t5_47_plus.touchscreen";

namespace {
inline bool i2c_ok(i2c::ErrorCode err, LilygoT547PlusTouchscreen *ts) {
  if (err != i2c::ERROR_OK) {
    ts->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  return true;
}
}  // namespace

void LilygoT547PlusTouchscreen::setup() {
  // GPIO47 wakeup sequence — required by this board for GT911 initialization.
  // Matches the LilyGo reference: TouchDrvGT911::wakeup()  // NOLINT
  //   pinMode(_irq, OUTPUT); digitalWrite(_irq, HIGH); delay(8); pinMode(_irq, INPUT);  // NOLINT
  pinMode(TOUCH_INT_PIN, OUTPUT);    // NOLINT
  digitalWrite(TOUCH_INT_PIN, HIGH);  // NOLINT
  delay(10);
  pinMode(TOUCH_INT_PIN, INPUT);  // NOLINT
  ESP_LOGD(TAG, "GT911 wakeup: GPIO%d HIGH→INPUT done", TOUCH_INT_PIN);

  // Verify I2C communication.
  // The GT911 I2C address (0x5D or 0x14) is latched at hardware reset depending
  // on the INT pin state. If the wakeup sequence above ran too late (e.g. after
  // a soft-reset without a full power cycle), the chip may be at the alternate
  // address. Try the fallback automatically so tinkerers don't get stuck.
  if (this->write(nullptr, 0) != i2c::ERROR_OK) {
    uint8_t original = this->address_;
    uint8_t fallback = (original == 0x5D) ? 0x14 : 0x5D;
    ESP_LOGW(TAG, "GT911 not found at 0x%02X, trying fallback 0x%02X", original, fallback);
    this->set_i2c_address(fallback);
    if (this->write(nullptr, 0) != i2c::ERROR_OK) {
      this->set_i2c_address(original);
      ESP_LOGE(TAG, "GT911 not found at 0x%02X or 0x%02X - power-cycle the board", original, fallback);
      this->mark_failed();
      return;
    }
    ESP_LOGI(TAG, "GT911 found at fallback address 0x%02X", fallback);
  }

  // Read interrupt mode from GT911 config register (0x804D)
  uint8_t switches;
  i2c::ErrorCode err = this->write(GET_SWITCHES, sizeof(GET_SWITCHES));
  if (err == i2c::ERROR_OK) {
    err = this->read(&switches, 1);
    if (err == i2c::ERROR_OK) {
      ESP_LOGD(TAG, "GT911 switches: 0x%02X (INT mode: %s)", switches, (switches & 1) ? "falling" : "rising");
    }
  }

  // Read max X/Y from GT911 if not configured
  if (this->x_raw_max_ == 0 || this->y_raw_max_ == 0) {
    uint8_t data[4];
    err = this->write(GET_MAX_VALUES, sizeof(GET_MAX_VALUES));
    if (err == i2c::ERROR_OK) {
      err = this->read(data, sizeof(data));
      if (err == i2c::ERROR_OK) {
        this->x_raw_max_ = encode_uint16(data[1], data[0]);
        this->y_raw_max_ = encode_uint16(data[3], data[2]);
        if (this->swap_x_y_)
          std::swap(this->x_raw_max_, this->y_raw_max_);
        ESP_LOGD(TAG, "GT911 max values: x=%d, y=%d", this->x_raw_max_, this->y_raw_max_);
      }
    }
    if (err != i2c::ERROR_OK) {
      ESP_LOGW(TAG, "Could not read calibration data, using defaults");
    }
  }

  ESP_LOGCONFIG(TAG, "GT911 touchscreen initialized (polling mode, no interrupt)");
}

void LilygoT547PlusTouchscreen::update_touches() {
  i2c::ErrorCode err;
  uint8_t touch_state = 0;
  uint8_t data[MAX_TOUCHES][8];

  err = this->write(GET_TOUCH_STATE, sizeof(GET_TOUCH_STATE));
  if (!i2c_ok(err, this))
    return;
  err = this->read(&touch_state, 1);
  if (!i2c_ok(err, this))
    return;
  this->write(CLEAR_TOUCH_STATE, sizeof(CLEAR_TOUCH_STATE));

  uint8_t num_of_touches = touch_state & 0x07;

  if ((touch_state & 0x80) == 0 || num_of_touches > MAX_TOUCHES) {
    return;
  }

  if (num_of_touches == 0) {
    return;
  }

  err = this->write(GET_TOUCHES, sizeof(GET_TOUCHES));
  if (!i2c_ok(err, this))
    return;
  err = this->read(data[0], sizeof(data[0]) * num_of_touches);
  if (!i2c_ok(err, this))
    return;

  for (uint8_t i = 0; i < num_of_touches; i++) {
    uint16_t id = data[i][0];
    uint16_t x = encode_uint16(data[i][2], data[i][1]);
    uint16_t y = encode_uint16(data[i][4], data[i][3]);
    this->add_raw_touch_position_(id, x, y);
  }

  this->status_clear_warning();
}

void LilygoT547PlusTouchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "LilyGo T5 4.7\" Plus Touchscreen (GT911):");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  INT Pin: GPIO%d (wakeup only, no interrupt)", TOUCH_INT_PIN);
}

}  // namespace lilygo_t5_47_plus
}  // namespace esphome
