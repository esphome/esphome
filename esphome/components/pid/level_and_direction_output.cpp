#include "level_and_direction_output.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pid {

static const char *const TAG = "pid.level_and_direction_output";

void LevelAndDirectionOutput::set_level(float level) {
  level = clamp(level, 0.0f, 1.0f);
  if (this->level_ != nullptr) {
    this->level_->set_level(level);
  }
}

void LevelAndDirectionOutput::set_reverse(bool reverse) {
  if (this->direction_ != nullptr) {
    this->direction_->set_state(reverse);
  }
}

void LevelAndDirectionOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "PID Level and direction output:");
  ESP_LOGCONFIG(TAG, "  Level output: %s", this->level_ ? "yes" : "no");
  ESP_LOGCONFIG(TAG, "  Direction output: %s", this->direction_ ? "yes" : "no");
}

}  // namespace pid
}  // namespace esphome
