#include "reporting_switch.h"

namespace esphome::ld2460 {

void ReportingSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_reporting(state);
}

}  // namespace esphome::ld2460
