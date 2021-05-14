#include "simpleevse_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace simpleevse {

void SimpleEvseChargingSwitch::update(bool running, const std::array<uint16_t, COUNT_STATUS_REGISTER> &status_register) {
  // do not update as long a write is pending
  if (!this->active_writes_) {
    this->publish_state((status_register[REGISTER_CTRL_BITS] & CHARGING_ENABLED_MASK) == CHARGING_ENABLED_ON);
  }
}

void SimpleEvseChargingSwitch::write_state(bool state) {
  this->active_writes_++;
  ESP_LOGD(TAG, "Queue command to change charging to %d. (Active writes=%d)", state, this->active_writes_);
  auto trans = make_unique<ModbusWriteHoldingRegistersTransaction>(FIRST_STATUS_REGISTER + REGISTER_CTRL_BITS, state ? CHARGING_ENABLED_ON : CHARGING_ENABLED_OFF, [this, state](ModbusTransactionResult result) {
    this->active_writes_--;
    ESP_LOGD(TAG, "Command executed. (Success=%d, Active writes=%d)", static_cast<int>(result), this->active_writes_);
    if (result == ModbusTransactionResult::SUCCESS) {
      this->publish_state(state);
    } else {
      this->publish_state(this->state);
    }
  });
  this->parent_->add_transaction(std::move(trans));
}

}  // namespace simpleevse
}  // namespace esphome
