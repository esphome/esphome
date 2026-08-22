#pragma once

#include "esphome/core/preferences.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::preferences {

class IntervalSyncer final : public PollingComponent {
 public:
  // Remove before 2027.3.0
  ESPDEPRECATED("Use set_update_interval() instead. Removed in 2027.3.0", "2026.9.0")
  void set_write_interval(uint32_t write_interval) { this->set_update_interval(write_interval); }
  void update() override {
    if (global_preferences->sync()) {
      this->status_clear_warning();
    } else {
      this->status_set_warning("Failed to sync preferences to flash");
    }
  }
  void on_shutdown() override {
    if (!global_preferences->sync()) {
      this->status_set_warning("Failed to sync preferences to flash");
    }
  }
  float get_setup_priority() const override { return setup_priority::BUS; }
};

}  // namespace esphome::preferences
