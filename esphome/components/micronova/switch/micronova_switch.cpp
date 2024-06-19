#include "micronova_switch.h"

namespace esphome {
namespace micronova {

void MicroNovaSwitch::write_state(bool state) {
  switch (this->get_function()) {
    case MicroNovaFunctions::STOVE_FUNCTION_SWITCH:
      if (state) {
        // Only send power-on when current state is Off
        if (this->micronova_->get_current_stove_state() == 0) {
          this->micronova_->write_address(this->memory_location_, this->memory_address_, this->memory_data_on_);
          this->publish_state(true);
        } else
          ESP_LOGW(TAG, "Unable to turn stove on, invalid state: %d", micronova_->get_current_stove_state());
      } else {
        // don't send power-off when status is Off or Final cleaning
        if (this->micronova_->get_current_stove_state() != 0 && micronova_->get_current_stove_state() != 6) {
          this->micronova_->write_address(this->memory_location_, this->memory_address_, this->memory_data_off_);
          this->publish_state(false);
        } else
          ESP_LOGW(TAG, "Unable to turn stove off, invalid state: %d", micronova_->get_current_stove_state());
      }
      this->micronova_->update();
      break;

    default:
      break;
  }
}

}  // namespace micronova
}  // namespace esphome
