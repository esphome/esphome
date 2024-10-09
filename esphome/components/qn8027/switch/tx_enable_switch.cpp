#include "tx_enable_switch.h"

namespace esphome {
namespace qn8027 {

void TxEnableSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_tx_enable(value);
}

}  // namespace qn8027
}  // namespace esphome
