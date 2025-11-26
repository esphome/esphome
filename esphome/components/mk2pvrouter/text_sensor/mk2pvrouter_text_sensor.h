#pragma once

#include "esphome/components/mk2pvrouter/mk2pvrouter.h"
#include "esphome/components/text_sensor/text_sensor.h"
namespace esphome {
namespace mk2pvrouter {
class Mk2PVRouterTextSensor : public Mk2PVRouterListener, public text_sensor::TextSensor, public Component {
 public:
  explicit Mk2PVRouterTextSensor(const char *tag);
  void publish_val(const std::string &val) override;
  void dump_config() override;
};
}  // namespace mk2pvrouter
}  // namespace esphome
