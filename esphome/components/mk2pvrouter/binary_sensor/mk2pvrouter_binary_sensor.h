#pragma once

#include "esphome/components/mk2pvrouter/mk2pvrouter.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::mk2pvrouter {

class Mk2PVRouterBinarySensor : public Mk2PVRouterListener, public binary_sensor::BinarySensor, public Component {
 public:
  explicit Mk2PVRouterBinarySensor(const char *tag);
  void publish_val(const char *val) override;
  void dump_config() override;
};

}  // namespace esphome::mk2pvrouter
