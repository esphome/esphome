#include "mildew_proof_switch.h"

namespace esphome::hitachi_ac344 {

void MildewProofSwitch::write_state(bool state) { this->parent_->set_mildew_proof(state); }

}  // namespace esphome::hitachi_ac344
