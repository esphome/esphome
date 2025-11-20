#include "logger_level_select.h"

namespace esphome::logger {

void LoggerLevelSelect::publish_state(int level) {
  auto index = level_to_index(level);
  if (!this->has_index(index))
    return;
  Select::publish_state(index);
}

void LoggerLevelSelect::setup() {
  this->parent_->add_listener([this](int level) { this->publish_state(level); });
  this->publish_state(this->parent_->get_log_level());
}

void LoggerLevelSelect::control(size_t index) { this->parent_->set_log_level(index_to_level(index)); }

}  // namespace esphome::logger
