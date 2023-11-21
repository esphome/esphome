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

void EKTF2232TouchscreenStore::gpio_intr(EKTF2232TouchscreenStore *store) { store->touch = true; }

void EKTF2232Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up EKT2232 Touchscreen...");
  this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->interrupt_pin_->setup();

  this->store_.pin = this->interrupt_pin_->to_isr();
  this->interrupt_pin_->attach_interrupt(EKTF2232TouchscreenStore::gpio_intr, &this->store_,
                                         gpio::INTERRUPT_FALLING_EDGE);

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
  this->write(GET_X_RES, 4);
  if (this->read(received, 4)) {
    ESP_LOGE(TAG, "Failed to read X resolution!");
    this->interrupt_pin_->detach_interrupt();
    this->mark_failed();
    return;
  }
  this->x_resolution_ = ((received[2])) | ((received[3] & 0xf0) << 4);

  this->write(GET_Y_RES, 4);
  if (this->read(received, 4)) {
    ESP_LOGE(TAG, "Failed to read Y resolution!");
    this->interrupt_pin_->detach_interrupt();
    this->mark_failed();
    return;
  }
  this->y_resolution_ = ((received[2])) | ((received[3] & 0xf0) << 4);
  this->store_.touch = false;

  this->set_power_state(true);
}

void EKTF2232Touchscreen::loop() {
  if (!this->store_.touch)
    return;
  this->store_.touch = false;

  uint8_t touch_count = 0;
  std::vector<TouchPoint> touches;

  uint8_t raw[8];
  this->read(raw, 8);
  for (int i = 0; i < 8; i++) {
    if (raw[7] & (1 << i))
      touch_count++;
  }

  if (touch_count == 0) {
    for (auto *listener : this->touch_listeners_)
      listener->release();
    return;
  }

  touch_count = std::min<uint8_t>(touch_count, 2);

  ESP_LOGV(TAG, "Touch count: %d", touch_count);

  for (int i = 0; i < touch_count; i++) {
    uint8_t *d = raw + 1 + (i * 3);
    uint32_t raw_x = (d[0] & 0xF0) << 4 | d[1];
    uint32_t raw_y = (d[0] & 0x0F) << 8 | d[2];

    raw_x = raw_x * this->display_height_ - 1;
    raw_y = raw_y * this->display_width_ - 1;

    TouchPoint tp;
    switch (this->rotation_) {
      case ROTATE_0_DEGREES:
        tp.y = raw_x / this->x_resolution_;
        tp.x = this->display_width_ - 1 - (raw_y / this->y_resolution_);
        break;
      case ROTATE_90_DEGREES:
        tp.x = raw_x / this->x_resolution_;
        tp.y = raw_y / this->y_resolution_;
        break;
      case ROTATE_180_DEGREES:
        tp.y = this->display_height_ - 1 - (raw_x / this->x_resolution_);
        tp.x = raw_y / this->y_resolution_;
        break;
      case ROTATE_270_DEGREES:
        tp.x = this->display_height_ - 1 - (raw_x / this->x_resolution_);
        tp.y = this->display_width_ - 1 - (raw_y / this->y_resolution_);
        break;
    }

    this->defer([this, tp]() { this->send_touch_(tp); });
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
  this->store_.touch = false;
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
  while (!this->store_.touch && timeout > 0) {
    delay(1);
    timeout--;
  }
  if (timeout > 0)
    this->store_.touch = true;
  this->read(received, 4);
  this->store_.touch = false;

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
