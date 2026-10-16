#include "dfrobot_sen0395_switch.h"

namespace esphome::dfrobot_sen0395 {

void Sen0395PowerSwitch::write_state(bool state) { this->parent_->enqueue(CommandType::COMMAND_TYPE_POWER, state); }

void Sen0395LedSwitch::write_state(bool state) {
  this->parent_->enqueue_saved_setting(CommandType::COMMAND_TYPE_LED_MODE, state);
}

void Sen0395UartPresenceSwitch::write_state(bool state) {
  this->parent_->enqueue_saved_setting(CommandType::COMMAND_TYPE_UART_OUTPUT, state);
}

void Sen0395StartAfterBootSwitch::write_state(bool state) {
  this->parent_->enqueue_saved_setting(CommandType::COMMAND_TYPE_SENSOR_CFG_START, state);
}

}  // namespace esphome::dfrobot_sen0395
