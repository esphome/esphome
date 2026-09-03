#include "logger_level_select.h"

namespace esphome::logger {

void LoggerLevelSelect::on_log_level_change(uint8_t level) {
  auto index = level_to_index(level);
  if (!this->has_index(index))
    return;
  Select::publish_state(index);
}

void LoggerLevelSelect::setup() {
  this->parent_->add_level_listener(this);
  this->on_log_level_change(this->parent_->get_log_level());
}

void LoggerLevelSelect::control(size_t index) { this->parent_->set_log_level(index_to_level(index)); }

}  // namespace esphome::logger
