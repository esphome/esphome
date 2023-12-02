#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace uptime {

class UptimeSensor : public sensor::Sensor, public PollingComponent {
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
