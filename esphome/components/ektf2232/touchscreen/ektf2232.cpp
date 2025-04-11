#include "ektf2232.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <vector>

namespace esphome {
namespace ektf2232 {

static const char *const TAG = "ektf2232";

static const uint8_t SOFT_RESET_CMD[4] = {0x77, 0x77, 0x77, 0x77};
static const uint8_t HELLO[4] = {0x55, 0x55, 0x55, 0x55};
static const uint8_t GET_X_RES[4] = {0x53, 0x60, 0x00, 0x00};
static const uint8_t GET_Y_RES[4] = {0x53, 0x63, 0x00, 0x00};
static const uint8_t GET_POWER_STATE_CMD[4] = {0x53, 0x50, 0x00, 0x01};

void EKTF2232Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up EKT2232 Touchscreen...");
  this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->interrupt_pin_->setup();

  this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);

  this->rts_pin_->setup();

  this->hard_reset_();
  if (!this->soft_reset_()) {
    ESP_LOGE(TAG, "Failed to soft reset EKT2232!");
    this->interrupt_pin_->detach_interrupt();
    this->mark_failed();
    return;
  }

  // Get touch resolution
  uint8_t received[4];
  if (this->x_raw_max_ == 0 || this->y_raw_max_ == 0) {
    auto err = this->write(GET_X_RES, 4);
    if (err == i2c::ERROR_OK) {
      err = this->read(received, 4);
      if (err == i2c::ERROR_OK) {
        this->x_raw_max_ = ((received[2])) | ((received[3] & 0xf0) << 4);
        err = this->write(GET_Y_RES, 4);
        if (err == i2c::ERROR_OK) {
          err = this->read(received, 4);
          if (err == i2c::ERROR_OK) {
            this->y_raw_max_ = ((received[2])) | ((received[3] & 0xf0) << 4);
          }
        }
      }
    }
    if (err != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "Failed to read calibration values!");
      this->interrupt_pin_->detach_interrupt();
      this->mark_failed();
      return;
    }
    if (this->swap_x_y_)
      std::swap(this->x_raw_max_, this->y_raw_max_);
  }
  this->set_power_state(true);
}

void EKTF2232Touchscreen::update_touches() {
  uint8_t touch_count = 0;
  int16_t x_raw, y_raw;

  uint8_t raw[8];
  this->read(raw, 8);
  for (int i = 0; i < 8; i++) {
    if (raw[7] & (1 << i))
      touch_count++;
  }

  touch_count = std::min<uint8_t>(touch_count, 2);

  ESP_LOGV(TAG, "Touch count: %d", touch_count);

  for (int i = 0; i < touch_count; i++) {
    uint8_t *d = raw + 1 + (i * 3);
    x_raw = (d[0] & 0xF0) << 4 | d[1];
    y_raw = (d[0] & 0x0F) << 8 | d[2];
    this->add_raw_touch_position_(i, x_raw, y_raw);
  }
}

void EKTF2232Touchscreen::set_power_state(bool enable) {
  uint8_t data[] = {0x54, 0x50, 0x00, 0x01};
  data[1] |= (enable << 3);
  this->write(data, 4);
}

bool EKTF2232Touchscreen::get_power_state() {
  uint8_t received[4];
  this->write(GET_POWER_STATE_CMD, 4);
  this->store_.touched = false;
  this->read(received, 4);
  return (received[1] >> 3) & 1;
}

void EKTF2232Touchscreen::hard_reset_() {
  this->rts_pin_->digital_write(false);
  delay(15);
  this->rts_pin_->digital_write(true);
  delay(15);
}

bool EKTF2232Touchscreen::soft_reset_() {
  auto err = this->write(SOFT_RESET_CMD, 4);
  if (err != i2c::ERROR_OK)
    return false;

  uint8_t received[4];
  uint16_t timeout = 1000;
  while (!this->store_.touched && timeout > 0) {
    delay(1);
    timeout--;
  }
  if (timeout > 0)
    this->store_.touched = true;
  this->read(received, 4);
  this->store_.touched = false;

  return !memcmp(received, HELLO, 4);
}

void EKTF2232Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "EKT2232 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  RTS Pin: ", this->rts_pin_);
}

}  // namespace ektf2232
}  // namespace esphome
