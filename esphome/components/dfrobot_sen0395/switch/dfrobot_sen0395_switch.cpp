#include "dfrobot_sen0395_switch.h"

namespace esphome {
namespace dfrobot_sen0395 {

void Sen0395PowerSwitch::write_state(bool state) { this->parent_->enqueue(make_unique<PowerCommand>(state)); }

void Sen0395LedSwitch::write_state(bool state) {
  bool wasActive = false;
  if (this->parent_->is_active()) {
    wasActive = true;
    this->parent_->enqueue(make_unique<PowerCommand>(false));
  }
  this->parent_->enqueue(make_unique<LedModeCommand>(state));
  this->parent_->enqueue(make_unique<SaveCfgCommand>());
  if (wasActive) {
    this->parent_->enqueue(make_unique<PowerCommand>(true));
  }
}

void Sen0395UartPresenceSwitch::write_state(bool state) {
  bool wasActive = false;
  if (this->parent_->is_active()) {
    wasActive = true;
    this->parent_->enqueue(make_unique<PowerCommand>(false));
  }
  this->parent_->enqueue(make_unique<UartOutputCommand>(state));
  this->parent_->enqueue(make_unique<SaveCfgCommand>());
  if (wasActive) {
    this->parent_->enqueue(make_unique<PowerCommand>(true));
  }
}

void Sen0395StartAfterBootSwitch::write_state(bool state) {
  bool wasActive = false;
  if (this->parent_->is_active()) {
    wasActive = true;
    this->parent_->enqueue(make_unique<PowerCommand>(false));
  }
  this->parent_->enqueue(make_unique<SensorCfgStartCommand>(state));
  this->parent_->enqueue(make_unique<SaveCfgCommand>());
  if (wasActive) {
    this->parent_->enqueue(make_unique<PowerCommand>(true));
  }
}

}  // namespace dfrobot_sen0395
}  // namespace esphome
