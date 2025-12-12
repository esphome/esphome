#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "esphome/components/logger/logger.h"

namespace esphome::logger {
class LoggerLevelSelect : public Component, public select::Select, public Parented<Logger> {
 public:
  void publish_state(int level);
  void setup() override;
  void control(size_t index) override;

 protected:
  // Convert log level to option index (skip CONFIG at level 4)
  static uint8_t level_to_index(uint8_t level) { return (level > ESPHOME_LOG_LEVEL_CONFIG) ? level - 1 : level; }
  // Convert option index to log level (skip CONFIG at level 4)
  static uint8_t index_to_level(uint8_t index) { return (index >= ESPHOME_LOG_LEVEL_CONFIG) ? index + 1 : index; }
};
}  // namespace esphome::logger
