#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sx1509 {

static const char *TAG = "sx1509_gpio_pin";

SX1509GPIOPin::SX1509GPIOPin(SX1509Component *parent, uint8_t pin, uint8_t mode, bool inverted)
    : GPIOPin(pin, mode, inverted), parent_(parent) {}
void SX1509GPIOPin::setup() { this->pin_mode(this->mode_); }
void SX1509GPIOPin::pin_mode(uint8_t mode) { this->parent_->pin_mode(this->pin_, mode); }
bool SX1509GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void SX1509GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }

}  // namespace sx1509
}  // namespace esphome