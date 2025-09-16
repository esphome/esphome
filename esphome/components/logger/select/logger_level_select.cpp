#include "logger_level_select.h"

#include "esphome/core/log.h"

namespace esphome::logger {

static const char *const TAG = "logger.select";

void LoggerLevelSelect::publish_state(int level) {
  for (uint8_t i = 0; i < this->levels_length_; i++) {
    if (this->levels_[i] == level) {
      const auto &option = this->at(i).value();
      select::Select::publish_state(option);
      return;
    }
  }
}

void LoggerLevelSelect::setup() {
  this->parent_->add_listener([this](int level) { this->publish_state(level); });
  this->publish_state(this->parent_->get_log_level());
}

void LoggerLevelSelect::control(const std::string &value) {
  // Find selected value in available log levels
  const auto index = this->index_of(value).value();
  this->parent_->set_log_level(this->levels_[index]);
}

}  // namespace esphome::logger
