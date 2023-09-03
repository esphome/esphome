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
static const uint8_t FT63X6_ADDR_TOUCH_COUNT = 0x02;

static const uint8_t FT63X6_ADDR_TOUCH1_ID = 0x05;
static const uint8_t FT63X6_ADDR_TOUCH1_X = 0x03;
static const uint8_t FT63X6_ADDR_TOUCH1_Y = 0x05;

static const uint8_t FT63X6_ADDR_TOUCH2_ID = 0x0B;
static const uint8_t FT63X6_ADDR_TOUCH2_X = 0x09;
static const uint8_t FT63X6_ADDR_TOUCH2_Y = 0x0B;

namespace esphome {
namespace ft63x6 {

static const char *const TAG = "FT63X6Touchscreen";

void FT63X6TouchscreenStore::gpio_intr(FT63X6TouchscreenStore *store) { store->touch = true; }

void FT63X6Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up FT63X6Touchscreen Touchscreen...");
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    this->interrupt_pin_->setup();
    this->store_.pin = this->interrupt_pin_->to_isr();
    this->interrupt_pin_->attach_interrupt(FT63X6TouchscreenStore::gpio_intr, &this->store_,
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

void FT63X6Touchscreen::loop() {
  if (this->interrupt_pin_ == nullptr || !this->store_.touch)
    return;
  this->store_.touch = false;
  check_touch_();
}

void FT63X6Touchscreen::update() {
  if (this->interrupt_pin_ == nullptr)
    check_touch_();
}

void FT63X6Touchscreen::check_touch_() {
  touch_point_.touch_count = read_touch_count_();

  if (touch_point_.touch_count == 0) {
    if (touched_) {
      touched_ = false;
      for (auto *listener : this->touch_listeners_)
        listener->release();
    }
    return;
  }

  touched_ = true;

  uint8_t touch_id = read_touch_id_(FT63X6_ADDR_TOUCH1_ID);  // id1 = 0 or 1
  uint8_t first_touch_id = touch_id;
  touch_point_.tp[touch_id].status =
      (touch_point_.tp[touch_id].status == TouchStatusEnum::RELEASE) ? TouchStatusEnum::TOUCH : TouchStatusEnum::REPEAT;
  touch_point_.tp[touch_id].x = read_touch_coordinate_(FT63X6_ADDR_TOUCH1_X);
  touch_point_.tp[touch_id].y = read_touch_coordinate_(FT63X6_ADDR_TOUCH1_Y);

  if (touch_point_.touch_count >= 2) {
    touch_id = read_touch_id_(FT63X6_ADDR_TOUCH2_ID);  // id2 = 0 or 1(~id1 & 0x01)
    touch_point_.tp[touch_id].status = (touch_point_.tp[touch_id].status == TouchStatusEnum::RELEASE)
                                           ? TouchStatusEnum::TOUCH
                                           : TouchStatusEnum::REPEAT;
    touch_point_.tp[touch_id].x = read_touch_coordinate_(FT63X6_ADDR_TOUCH2_X);
    touch_point_.tp[touch_id].y = read_touch_coordinate_(FT63X6_ADDR_TOUCH2_Y);
    first_touch_id = 0;
  } else {
    touch_point_.tp[~touch_id & 0x01].status = TouchStatusEnum::RELEASE;
  }

  uint8_t touch_count = std::min<uint8_t>(touch_point_.touch_count, 2);
  ESP_LOGV(TAG, "Touch count: %d", touch_count);

  uint16_t w = this->display_->get_width();
  uint16_t h = this->display_->get_height();
  auto rotation = static_cast<TouchRotation>(this->display_->get_rotation());
  uint16_t x_resolution, y_resolution;
    switch (rotation) {
      case ROTATE_0_DEGREES:
      case ROTATE_180_DEGREES:
        x_resolution = this->x_resolution_;
        y_resolution = this->y_resolution_;
        break;

      case ROTATE_90_DEGREES:
      case ROTATE_270_DEGREES:
        x_resolution = this->y_resolution_;
        y_resolution = this->x_resolution_;
        break;
    }

  for (uint8_t i = first_touch_id; i < (touch_count + first_touch_id); i++) {
    uint32_t raw_x = touch_point_.tp[i].x * w / x_resolution;
    uint32_t raw_y = touch_point_.tp[i].y * h / y_resolution;

    TouchPoint tp;
    switch (rotation) {
      case ROTATE_0_DEGREES:
        tp.x = raw_x;
        tp.y = raw_y;
        break;
      case ROTATE_90_DEGREES:
        tp.x = raw_y;
        tp.y = h - std::min<uint32_t>(raw_x, h);
        break;
      case ROTATE_180_DEGREES:
        tp.x = w - std::min<uint32_t>(raw_x, w);
        tp.y = h - std::min<uint32_t>(raw_y, h);
        break;
      case ROTATE_270_DEGREES:
        tp.x = w - std::min<uint32_t>(raw_y, w);
        tp.y = raw_x;
        break;
    }

    tp.id = i;
    tp.state = (uint8_t) touch_point_.tp[i].status;
    this->defer([this, tp]() { this->send_touch_(tp); });
  }
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
}

uint8_t FT63X6Touchscreen::read_touch_count_() { return read_byte_(FT63X6_ADDR_TOUCH_COUNT); }

// Touch functions
uint16_t FT63X6Touchscreen::read_touch_coordinate_(uint8_t coordinate) {
  uint8_t read_buf[2];
  read_buf[0] = read_byte_(coordinate);
  read_buf[1] = read_byte_(coordinate + 1);
  return ((read_buf[0] & 0x0f) << 8) | read_buf[1];
}
uint8_t FT63X6Touchscreen::read_touch_id_(uint8_t id_address) { return read_byte_(id_address) >> 4; }

uint8_t FT63X6Touchscreen::read_byte_(uint8_t addr) {
  uint8_t byte = 0;
  this->read_byte(addr, &byte);
  return byte;
}

}  // namespace ft63x6
}  // namespace esphome
