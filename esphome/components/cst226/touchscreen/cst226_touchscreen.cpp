#include "cst226_touchscreen.h"

namespace esphome {
namespace cst226 {

void CST226Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "CST226 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  const char *name;
}

}  // namespace cst226
}  // namespace esphome
