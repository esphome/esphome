#include "sensitivity_select.h"

namespace esphome {
namespace seeed_mr60fda2 {

void SensitivitySelect::control(const std::string &value) {
  this->publish_state(value);
  auto index = this->index_of(value);
  if (index.has_value()) {
    this->parent_->set_sensitivity(index.value());
  }
}

}  // namespace seeed_mr60fda2
}  // namespace esphome
