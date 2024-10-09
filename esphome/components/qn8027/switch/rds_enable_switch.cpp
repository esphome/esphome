#include "rds_enable_switch.h"

namespace esphome {
namespace qn8027 {

void RDSEnableSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_rds_enable(value);
}

}  // namespace qn8027
}  // namespace esphome
