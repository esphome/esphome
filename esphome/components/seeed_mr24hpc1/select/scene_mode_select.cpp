#include "scene_mode_select.h"

namespace esphome {
namespace seeed_mr24hpc1 {

void SceneModeSelect::control(const std::string &value) { this->parent_->set_scene_mode(value); }

}  // namespace seeed_mr24hpc1
}  // namespace esphome
