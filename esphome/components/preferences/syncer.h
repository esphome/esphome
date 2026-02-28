#pragma once

#include "esphome/core/preferences.h"
#include "esphome/core/component.h"

namespace esphome {
namespace preferences {

class IntervalSyncer : public PollingComponent {
 public:
  void set_write_interval(uint32_t write_interval) { this->set_update_interval(write_interval); }
  void update() override { global_preferences->sync(); }
  void setup() override {
    if (this->update_interval_ == 0) {
      this->stop_poller();
    } else {
      // When using interval-based syncing, we don't need the loop
      this->disable_loop();
    }
  }
  void loop() override {
    if (this->update_interval_ == 0) {
      global_preferences->sync();
    }
  }
  void on_shutdown() override { global_preferences->sync(); }
  float get_setup_priority() const override { return setup_priority::BUS; }
};

}  // namespace preferences
}  // namespace esphome
