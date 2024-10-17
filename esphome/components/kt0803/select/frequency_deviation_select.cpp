#include "frequency_deviation_select.h"

namespace esphome {
namespace kt0803 {

void FrequencyDeviationSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_frequency_deviation((FrequencyDeviation) *index);
  }
}

}  // namespace kt0803
}  // namespace esphome
