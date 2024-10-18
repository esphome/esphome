#include "silence_detection_switch.h"

namespace esphome {
namespace kt0803 {

void SilenceDetectionSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_silence_detection(value);
}

}  // namespace kt0803
}  // namespace esphome
