#include "i80_component.h"
#ifdef USE_I80

namespace esphome {
namespace i80 {
void I80Component::dump_config() {
  ESP_LOGCONFIG(TAG, "I80 bus:");
  LOG_PIN("  WR Pin: ", this->wr_pin_)
  LOG_PIN("  DC Pin: ", this->dc_pin_)
  for (unsigned i = 0; i != this->data_pins_.size(); i++) {
    ESP_LOGCONFIG(TAG, "  Data pin %u: GPIO%d", i, this->data_pins_[i]);
  }
}

}  // namespace i80
}  // namespace esphome

#endif  // USE_I80
