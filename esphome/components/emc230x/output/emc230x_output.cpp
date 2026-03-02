#include "emc230x_output.h"

namespace esphome::emc230x {

void Emc230xOutput::write_state(float state) { this->parent_->set_duty_cycle(this->fan_, state); }

}  // namespace esphome::emc230x
