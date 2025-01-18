#pragma once

#include "esphome/core/defines.h"

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace uptime {

class UptimeTextSensor : public text_sensor::TextSensor, public PollingComponent {
 public:
  void update() override;
  void dump_config() override;
  void setup() override;

  float get_setup_priority() const override;

 protected:
  uint64_t uptime_{0};
  uint64_t last_ms_{0};
};

}  // namespace uptime
}  // namespace esphome
