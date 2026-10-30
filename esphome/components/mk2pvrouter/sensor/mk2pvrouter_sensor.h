#pragma once

#include "esphome/components/mk2pvrouter/mk2pvrouter.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::mk2pvrouter {

class Mk2PVRouterSensor final : public Mk2PVRouterListener, public sensor::Sensor {
 public:
  Mk2PVRouterSensor(const char *tag, bool scale_centi);
  void publish_val(const char *val) override;

 protected:
  bool scale_centi_;
};

}  // namespace esphome::mk2pvrouter
