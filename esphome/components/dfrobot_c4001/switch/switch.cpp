#include "switch.h"

namespace esphome {
namespace dfrobot_c4001 {
void OutLedSwitch::write_state(bool state) { this->parent_->set_out_led_enable(state); }
void RunLedSwitch::write_state(bool state) { this->parent_->set_run_led_enable(state); }
void MicroMotionSwitch::write_state(bool state) { this->parent_->set_micro_motion_enable(state); }
}  // namespace dfrobot_c4001
}  // namespace esphome
