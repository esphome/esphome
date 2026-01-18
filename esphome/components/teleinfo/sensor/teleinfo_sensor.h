#pragma once
#include "esphome/components/teleinfo/teleinfo.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace teleinfo {

class TeleInfoSensor : public TeleInfoListener, public sensor::Sensor, public Component {
 public:
  TeleInfoSensor(const char *tag) { this->tag = tag; }
  void publish_val(const char *val) override;
  void dump_config() override;
};

}  // namespace teleinfo
}  // namespace esphome
