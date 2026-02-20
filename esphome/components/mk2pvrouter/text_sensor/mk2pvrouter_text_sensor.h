#pragma once

#include "esphome/components/mk2pvrouter/mk2pvrouter.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::mk2pvrouter {

class Mk2PVRouterTextSensor : public Mk2PVRouterListener, public text_sensor::TextSensor, public Component {
 public:
  explicit Mk2PVRouterTextSensor(const char *tag);
  void publish_val(const char *val) override;
  void dump_config() override;
};

}  // namespace esphome::mk2pvrouter
