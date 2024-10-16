#include "acomp_release_select.h"

namespace esphome {
namespace si4713 {

void AcompReleaseSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_acomp_release((AcompRelease) *index);
  }
}

}  // namespace si4713
}  // namespace esphome