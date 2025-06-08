#include "dac_output_select.h"

namespace esphome {
namespace es8388 {

void DacOutputSelect::control(const std::string &value) {
  this->publish_state(value);
  this->parent_->set_dac_output(static_cast<DacOutputLine>(this->index_of(value).value()));
}

}  // namespace es8388
}  // namespace esphome
