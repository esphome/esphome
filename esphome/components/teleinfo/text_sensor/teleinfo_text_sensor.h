#pragma once
#include "esphome/components/teleinfo/teleinfo.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::teleinfo {
class TeleInfoTextSensor final : public TeleInfoListener, public text_sensor::TextSensor {
 public:
  TeleInfoTextSensor(const char *tag);
  void publish_val(const std::string &val) override;
  void dump_child_config() override;
};
}  // namespace esphome::teleinfo
