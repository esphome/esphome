#include "logger_level_select.h"

namespace esphome {
namespace logger {

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
  auto level = this->index_of(value);
  if (!level)
    return;
  this->parent_->set_log_level(level.value());
}

}  // namespace logger
}  // namespace esphome
