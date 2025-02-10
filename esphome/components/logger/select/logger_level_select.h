#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "esphome/components/logger/logger.h"
namespace esphome {
namespace logger {
class LoggerLevelSelect : public Component, public select::Select, public Parented<Logger> {
 public:
  void publish_state(int level);
  void setup() override;
  void control(const std::string &value) override;
};
}  // namespace logger
}  // namespace esphome
