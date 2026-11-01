#pragma once

#include "esphome/components/mk2pvrouter/mk2pvrouter.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::mk2pvrouter {

// Component before Sensor so the flag lands in Sensor's tail padding
class Mk2PVRouterSensor final : public Mk2PVRouterListener, public Component, public sensor::Sensor {
 public:
  Mk2PVRouterSensor(const char *tag, bool scale_centi);
  void publish_val(const char *val) override;
  void dump_config() override;

 protected:
  bool scale_centi_;
};

}  // namespace esphome::mk2pvrouter
