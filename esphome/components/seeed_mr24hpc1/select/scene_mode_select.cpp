#include "scene_mode_select.h"

namespace esphome {
namespace seeed_mr24hpc1 {

void SceneModeSelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_scene_mode(index);
}

}  // namespace seeed_mr24hpc1
}  // namespace esphome
