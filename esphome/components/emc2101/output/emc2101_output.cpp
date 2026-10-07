#include "emc2101_output.h"

namespace esphome::emc2101 {

void EMC2101Output::write_state(float state) { this->parent_->set_duty_cycle(state); }

}  // namespace esphome::emc2101
