#include "cst816_touchscreen.h"

namespace esphome {
namespace cst816 {

void CST816Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "CST816 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  const char *name;
  switch (this->chip_id_) {
    case CST816S_CHIP_ID:
      name = "CST816S";
      break;
    case CST816D_CHIP_ID:
      name = "CST816D";
      break;
    case CST716_CHIP_ID:
      name = "CST716";
      break;
    case CST816T_CHIP_ID:
      name = "CST816T";
      break;
    default:
      name = "Unknown";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Chip type: %s", name);
}

}  // namespace cst816
}  // namespace esphome
