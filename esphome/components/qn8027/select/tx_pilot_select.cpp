#include "tx_pilot_select.h"

namespace esphome {
namespace qn8027 {

void TxPilotSelect::control(const std::string &value) {
  this->publish_state(value);
  this->parent_->set_tx_pilot(atoi(state.c_str()));
}

}  // namespace qn8027
}  // namespace esphome
