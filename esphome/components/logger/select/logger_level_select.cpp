#include "logger_level_select.h"

namespace esphome::logger {

void LoggerLevelSelect::publish_state(int level) {
  const auto &option = this->at(level_to_index(level));
  if (!option)
    return;
  Select::publish_state(option.value());
}

void LoggerLevelSelect::setup() {
  this->parent_->add_listener([this](int level) { this->publish_state(level); });
  this->publish_state(this->parent_->get_log_level());
}

void LoggerLevelSelect::control(const std::string &value) {
  const auto index = this->index_of(value);
  if (!index)
    return;
  this->parent_->set_log_level(index_to_level(index.value()));
}

}  // namespace esphome::logger
