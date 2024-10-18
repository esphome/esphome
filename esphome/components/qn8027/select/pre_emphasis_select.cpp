#include "pre_emphasis_select.h"

namespace esphome {
namespace qn8027 {

void PreEmphasisSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_pre_emphasis((PreEmphasis) *index);
  }
}

}  // namespace qn8027
}  // namespace esphome
