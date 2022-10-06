/**************************************************************************/
/*!
  Author: Gustavo Ambrozio
  Based on work by: Atsushi Sasaki (https://github.com/aselectroworks/Arduino-FT6336U)
*/
/**************************************************************************/

#include "ft6336u.h"
#include "esphome/core/log.h"

// Registers
static const uint8_t FT6336U_ADDR_TOUCH_COUNT = 0x02;

static const uint8_t FT6336U_ADDR_TOUCH1_ID = 0x05;
static const uint8_t FT6336U_ADDR_TOUCH1_X = 0x03;
static const uint8_t FT6336U_ADDR_TOUCH1_Y = 0x05;

static const uint8_t FT6336U_ADDR_TOUCH2_ID = 0x0B;
static const uint8_t FT6336U_ADDR_TOUCH2_X = 0x09;
static const uint8_t FT6336U_ADDR_TOUCH2_Y = 0x0B;

namespace esphome {
namespace ft6336u {

static const char *const TAG = "FT6336UTouchscreen";

void FT6336UTouchscreenStore::gpio_intr(FT6336UTouchscreenStore *store) { store->touch = true; }

void FT6336UTouchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up FT6336UTouchscreen Touchscreen...");
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    this->interrupt_pin_->setup();
    this->store_.pin = this->interrupt_pin_->to_isr();
    this->interrupt_pin_->attach_interrupt(FT6336UTouchscreenStore::gpio_intr, &this->store_, 
                                           gpio::INTERRUPT_FALLING_EDGE);
  }

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
  }

  this->hard_reset_();

  // Get touch resolution
  this->x_resolution_ = 320;
  this->y_resolution_ = 480;
}

void FT6336UTouchscreen::loop() {
  if (this->interrupt_pin_ == nullptr || !this->store_.touch)
    return;
  this->store_.touch = false;
  check_touch_();
}

void FT6336UTouchscreen::update() {
  if (this->interrupt_pin_ == nullptr)
    check_touch_();
}

void FT6336UTouchscreen::check_touch_() {
  touchPoint.touch_count = read_touch_count_();

  if (touchPoint.touch_count == 0) {
    touchPoint.tp[0].status = TouchStatusEnum::RELEASE;
    touchPoint.tp[1].status = TouchStatusEnum::RELEASE;
  } else if (touchPoint.touch_count == 1) {
    uint8_t id1 = read_touch1_id_(); // id1 = 0 or 1
    touchPoint.tp[id1].status = 
      (touchPoint.tp[id1].status == TouchStatusEnum::RELEASE) ? TouchStatusEnum::TOUCH : TouchStatusEnum::STREAM;
    touchPoint.tp[id1].x = read_touch1_x_();
    touchPoint.tp[id1].y = read_touch1_y_();
    touchPoint.tp[~id1 & 0x01].status = TouchStatusEnum::RELEASE;
  } else {
    uint8_t id1 = read_touch1_id_(); // id1 = 0 or 1
    touchPoint.tp[id1].status = 
      (touchPoint.tp[id1].status == TouchStatusEnum::RELEASE) ? TouchStatusEnum::TOUCH : TouchStatusEnum::STREAM;
    touchPoint.tp[id1].x = read_touch1_x_();
    touchPoint.tp[id1].y = read_touch1_y_();
    uint8_t id2 = read_touch2_id_(); // id2 = 0 or 1(~id1 & 0x01)
    touchPoint.tp[id2].status = 
      (touchPoint.tp[id2].status == TouchStatusEnum::RELEASE) ? TouchStatusEnum::TOUCH : TouchStatusEnum::STREAM;
    touchPoint.tp[id2].x = read_touch2_x_();
    touchPoint.tp[id2].y = read_touch2_y_();
  }

  if (touchPoint.touch_count == 0) {
    for (auto *listener : this->touch_listeners_)
      listener->release();
    return;
  }

  std::vector<TouchPoint> touches;
  uint8_t touch_count = std::min<uint8_t>(touchPoint.touch_count, 2);
  ESP_LOGV(TAG, "Touch count: %d", touch_count);

  uint16_t w = this->display_->get_width_internal();
  uint16_t h = this->display_->get_height_internal();

  for (int i = 0; i < touch_count; i++) {
    uint32_t raw_x = touchPoint.tp[i].x * w / this->x_resolution_;
    uint32_t raw_y = touchPoint.tp[i].y * h / this->y_resolution_;

    TouchPoint tp;
    switch (this->rotation_) {
      case ROTATE_0_DEGREES:
        tp.x = raw_x;
        tp.y = raw_y;
        break;
      case ROTATE_90_DEGREES:
        tp.x = raw_y;
        tp.y = w - std::min<uint32_t>(raw_x, w);
        break;
      case ROTATE_180_DEGREES:
        tp.x = w - std::min<uint32_t>(raw_x, w);
        tp.y = h - std::min<uint32_t>(raw_y, h);
        break;
      case ROTATE_270_DEGREES:
        tp.x = h - std::min<uint32_t>(raw_y, h);
        tp.y = raw_x;
        break;
    }

    this->defer([this, tp]() { this->send_touch_(tp); });
  }
}

void FT6336UTouchscreen::hard_reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
  }
}

void FT6336UTouchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "FT6336U Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

uint8_t FT6336UTouchscreen::read_touch_count_() { return read_byte_(FT6336U_ADDR_TOUCH_COUNT); }

// Touch 1 functions
uint16_t FT6336UTouchscreen::read_touch1_x_() {
  uint8_t read_buf[2];
  read_buf[0] = read_byte_(FT6336U_ADDR_TOUCH1_X);
  read_buf[1] = read_byte_(FT6336U_ADDR_TOUCH1_X + 1);
  return ((read_buf[0] & 0x0f) << 8) | read_buf[1];
}
uint16_t FT6336UTouchscreen::read_touch1_y_() {
  uint8_t read_buf[2];
  read_buf[0] = read_byte_(FT6336U_ADDR_TOUCH1_Y);
  read_buf[1] = read_byte_(FT6336U_ADDR_TOUCH1_Y + 1);
  return ((read_buf[0] & 0x0f) << 8) | read_buf[1];
}
uint8_t FT6336UTouchscreen::read_touch1_id_() { return read_byte_(FT6336U_ADDR_TOUCH1_ID) >> 4; }
// Touch 2 functions
uint16_t FT6336UTouchscreen::read_touch2_x_() {
  uint8_t read_buf[2];
  read_buf[0] = read_byte_(FT6336U_ADDR_TOUCH2_X);
  read_buf[1] = read_byte_(FT6336U_ADDR_TOUCH2_X + 1);
  return ((read_buf[0] & 0x0f) << 8) | read_buf[1];
}
uint16_t FT6336UTouchscreen::read_touch2_y_() {
  uint8_t read_buf[2];
  read_buf[0] = read_byte_(FT6336U_ADDR_TOUCH2_Y);
  read_buf[1] = read_byte_(FT6336U_ADDR_TOUCH2_Y + 1);
  return ((read_buf[0] & 0x0f) << 8) | read_buf[1];
}
uint8_t FT6336UTouchscreen::read_touch2_id_() { return read_byte_(FT6336U_ADDR_TOUCH2_ID) >> 4; }

uint8_t FT6336UTouchscreen::read_byte_(uint8_t addr) {
  uint8_t byte = 0;
  this->read_byte(addr, &byte);
  return byte;
}

}  // namespace ft6336u
}  // namespace esphome
