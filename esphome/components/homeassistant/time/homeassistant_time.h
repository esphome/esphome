#pragma once

#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/api/api_server.h"

namespace esphome {
namespace homeassistant {

class HomeassistantTime : public time::RealTimeClock {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void set_epoch_time(uint32_t epoch) { this->synchronize_epoch_(epoch); }
  float get_setup_priority() const override;
};

extern HomeassistantTime *global_homeassistant_time;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace homeassistant
}  // namespace esphome
