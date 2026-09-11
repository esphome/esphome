#include "dac_output_select.h"

namespace esphome::es8388 {

void DacOutputSelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_dac_output(static_cast<DacOutputLine>(index));
}

}  // namespace esphome::es8388
