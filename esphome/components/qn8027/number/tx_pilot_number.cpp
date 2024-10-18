#include "tx_pilot_number.h"

namespace esphome {
namespace qn8027 {

void TxPilotNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_tx_pilot((uint8_t) lround(value));
}

}  // namespace qn8027
}  // namespace esphome
