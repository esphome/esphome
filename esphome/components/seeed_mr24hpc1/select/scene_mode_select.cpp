#include "scene_mode_select.h"

namespace esphome {
namespace seeed_mr24hpc1 {

void SceneModeSelect::control(const std::string &value) {
  this->publish_state(value);
  auto index = this->index_of(value);
  if (index.has_value()) {
    this->parent_->set_scene_mode(index.value());
  }
}

}  // namespace seeed_mr24hpc1
}  // namespace esphome
