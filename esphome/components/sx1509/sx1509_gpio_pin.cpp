#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "sx1509_gpio_pin.h"

namespace esphome {
namespace sx1509 {

static const char *const TAG = "sx1509_gpio_pin";

void SX1509GPIOPin::setup() { pin_mode(flags_); }
void SX1509GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool SX1509GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void SX1509GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }
std::string SX1509GPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%u via sx1509", pin_);
  return buffer;
}

}  // namespace sx1509
}  // namespace esphome
