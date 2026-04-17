#pragma once

#include "esphome/core/preferences.h"
#include "esphome/core/component.h"

namespace esphome {
namespace preferences {

class IntervalSyncer final : public PollingComponent {
 public:
  void set_write_interval(uint32_t write_interval) { this->set_update_interval(write_interval); }
  void update() override { global_preferences->sync(); }
  void on_shutdown() override { global_preferences->sync(); }
  float get_setup_priority() const override { return setup_priority::BUS; }
};

}  // namespace preferences
}  // namespace esphome
