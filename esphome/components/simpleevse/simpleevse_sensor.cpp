#include "simpleevse_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace simpleevse {

void SimpleEvseSensors::update(bool running, const std::array<uint16_t, COUNT_STATUS_REGISTER> &status_register) {
  if (running) {
    if (this->set_charge_current_) {
      this->set_charge_current_->publish_state(status_register[REGISTER_CHARGE_CURRENT]);
    }

    if (this->actual_charge_current_) {
      this->actual_charge_current_->publish_state(status_register[REGISTER_ACTUAL_CURRENT]);
    }

    if (this->max_current_limit_) {
      this->max_current_limit_->publish_state(status_register[REGISTER_MAX_CURRENT]);
    }

    if (this->firmware_revision_) {
      this->firmware_revision_->publish_state(status_register[REGISTER_FIRMWARE]);
    }
  }
}

}  // namespace simpleevse
}  // namespace esphome
