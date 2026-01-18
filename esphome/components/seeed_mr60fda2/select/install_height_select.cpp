#include "install_height_select.h"

namespace esphome {
namespace seeed_mr60fda2 {

void InstallHeightSelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_install_height(index);
}

}  // namespace seeed_mr60fda2
}  // namespace esphome
