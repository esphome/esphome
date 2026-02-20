#pragma once

#include "esphome/components/mk2pvrouter/mk2pvrouter.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::mk2pvrouter {

class Mk2PVRouterSensor : public Mk2PVRouterListener, public sensor::Sensor, public Component {
 public:
  explicit Mk2PVRouterSensor(const char *tag);
  void publish_val(const char *val) override;
  void dump_config() override;
};

}  // namespace esphome::mk2pvrouter
