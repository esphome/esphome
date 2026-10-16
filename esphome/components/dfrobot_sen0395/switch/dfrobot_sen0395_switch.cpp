#include "dfrobot_sen0395_switch.h"

namespace esphome::dfrobot_sen0395 {

void Sen0395PowerSwitch::write_state(bool state) { this->parent_->enqueue(CommandType::COMMAND_TYPE_POWER, state); }

void Sen0395LedSwitch::write_state(bool state) {
  bool was_active = false;
  if (this->parent_->is_active()) {
    was_active = true;
    this->parent_->enqueue(CommandType::COMMAND_TYPE_POWER, false);
  }
  this->parent_->enqueue(CommandType::COMMAND_TYPE_LED_MODE, state);
  this->parent_->enqueue(CommandType::COMMAND_TYPE_SAVE_CFG);
  if (was_active) {
    this->parent_->enqueue(CommandType::COMMAND_TYPE_POWER, true);
  }
}

void Sen0395UartPresenceSwitch::write_state(bool state) {
  bool was_active = false;
  if (this->parent_->is_active()) {
    was_active = true;
    this->parent_->enqueue(CommandType::COMMAND_TYPE_POWER, false);
  }
  this->parent_->enqueue(CommandType::COMMAND_TYPE_UART_OUTPUT, state);
  this->parent_->enqueue(CommandType::COMMAND_TYPE_SAVE_CFG);
  if (was_active) {
    this->parent_->enqueue(CommandType::COMMAND_TYPE_POWER, true);
  }
}

void Sen0395StartAfterBootSwitch::write_state(bool state) {
  bool was_active = false;
  if (this->parent_->is_active()) {
    was_active = true;
    this->parent_->enqueue(CommandType::COMMAND_TYPE_POWER, false);
  }
  this->parent_->enqueue(CommandType::COMMAND_TYPE_SENSOR_CFG_START, state);
  this->parent_->enqueue(CommandType::COMMAND_TYPE_SAVE_CFG);
  if (was_active) {
    this->parent_->enqueue(CommandType::COMMAND_TYPE_POWER, true);
  }
}

}  // namespace esphome::dfrobot_sen0395
