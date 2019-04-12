#pragma once

#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/api/api_server.h"

namespace esphome {
namespace homeassistant {

class HomeassistantTime : public time::RealTimeClock {
 public:
  void setup() override;
  void dump_config() override;
  void set_epoch_time(uint32_t epoch) { this->synchronize_epoch_(epoch); }
  float get_setup_priority() const override;
};

extern HomeassistantTime *global_homeassistant_time;

class GetTimeResponse : public api::APIMessage {
 public:
  bool decode_32bit(uint32_t field_id, uint32_t value) override;
  api::APIMessageType message_type() const override;
};

}  // namespace homeassistant
}  // namespace esphome
