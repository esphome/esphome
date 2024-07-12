#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace uptime {

class UptimeSecondsSensor : public sensor::Sensor, public PollingComponent {
 public:
  void update() override;
  void dump_config() override;

  float get_setup_priority() const override;

  std::string unique_id() override;

 protected:
  uint64_t uptime_{0};
};

}  // namespace uptime
}  // namespace esphome
