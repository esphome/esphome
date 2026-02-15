#include "emc2303_output.h"

namespace esphome::emc2303 {

static const char *const TAG = "emc2303.component";

void Emc2303Output::write_state(float state) { this->parent_->set_duty_cycle(this->fan_, state); }

}  // namespace esphome::emc2303
