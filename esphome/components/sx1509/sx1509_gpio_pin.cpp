#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "sx1509_gpio_pin.h"

namespace esphome {
namespace sx1509 {

static const char *TAG = "sx1509_gpio_pin";

SX1509GPIOPin::SX1509GPIOPin(SX1509Component *parent, uint8_t pin, uint8_t mode, bool inverted, uint16_t t_on,
                             uint16_t t_off, uint16_t t_rise, uint16_t t_fall)
    : GPIOPin(pin, mode, inverted), parent_(parent) {
  t_on_ = t_on;
  t_off_ = t_off;
  t_rise_ = t_rise;
  t_fall_ = t_fall;
}
void SX1509GPIOPin::setup() {
  ESP_LOGD(TAG, "setup pin %d", this->pin_);
  this->pin_mode(this->mode_);
  switch (this->mode_) {
    case BREATHE_OUTPUT:
      this->parent_->breathe(this->pin_, t_on_, t_off_, t_rise_, t_fall_);
      break;
    case BLINK_OUTPUT:
      this->parent_->blink(this->pin_, t_on_, t_off_);
      break;
  }
}
void SX1509GPIOPin::pin_mode(uint8_t mode) { this->parent_->pin_mode(this->pin_, mode); }
bool SX1509GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void SX1509GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }

}  // namespace sx1509
}  // namespace esphome