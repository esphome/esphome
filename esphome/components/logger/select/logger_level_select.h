#pragma once

#include "esphome/components/logger/logger.h"
#include "esphome/components/select/select.h"
#include "esphome/core/component.h"

namespace esphome::logger {

class LoggerLevelSelect : public Component, public select::Select, public Parented<Logger> {
 public:
  LoggerLevelSelect(std::initializer_list<uint8_t> levels) {
    uint8_t length = levels.size();
    this->levels_ = make_unique<uint8_t[]>(length);
    std::memcpy(this->levels_.get(), levels.begin(), length);
    this->levels_length_ = length;
  }
  void publish_state(int level);
  void setup() override;
  void control(const std::string &value) override;

 protected:
  std::unique_ptr<uint8_t[]> levels_{nullptr};
  uint8_t levels_length_{0};
};

}  // namespace esphome::logger
