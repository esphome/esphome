#include "logger_level_select.h"

namespace esphome::logger {

void LoggerLevelSelect::publish_state(int level) {
  auto value = this->at(level);
  if (!value) {
    return;
  }
  Select::publish_state(value.value());
}

void LoggerLevelSelect::setup() {
  this->parent_->add_listener([this](int level) { this->publish_state(level); });
  this->publish_state(this->parent_->get_log_level());
}

void LoggerLevelSelect::control(const std::string &value) {
  // Find selected value in available log levels
  auto begin_it = std::begin(LOG_LEVELS);
  auto end_it = std::end(LOG_LEVELS);
  auto it = std::find_if(begin_it, end_it, [value](const char* x) {
        return strcmp(x, value.c_str()) == 0;
    });

  if (it == end_it) {
    return;
  }

  auto level = std::distance(begin_it, it);
  this->parent_->set_log_level(level);
}

}  // namespace esphome::logger
