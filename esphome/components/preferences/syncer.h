#pragma once

#include "esphome/core/preferences.h"
#include "esphome/core/component.h"

namespace esphome {
namespace preferences {

class IntervalSyncer final : public Component {
 public:
#ifdef USE_PREFERENCES_SYNC_EVERY_LOOP
  void loop() override { global_preferences->sync(); }
#else
  void set_write_interval(uint32_t write_interval) { this->write_interval_ = write_interval; }
  void setup() override {
    if (this->write_interval_ != 0) {
      this->start_poller();
      // When using interval-based syncing, we don't need the loop
      this->disable_loop();
    }
  }
  void loop() override {
    if (this->write_interval_ == 0) {
      global_preferences->sync();
    }
  }
#endif
  void on_shutdown() override { global_preferences->sync(); }
  float get_setup_priority() const override { return setup_priority::BUS; }
  void start_poller() {
    // Reregister interval.
    if (this->write_interval_ != 0) {
      this->set_interval(InternalSchedulerID::POLLING_UPDATE, this->write_interval_,
                         []() { global_preferences->sync(); });
    } else {
      // doesn't do anything if state is not COMPONENT_STATE_LOOP_DONE
      this->enable_loop();
    }
  }
  void stop_poller() {
    if (this->write_interval_ != 0) {
      // Clear the interval to suspend component
      this->cancel_interval(InternalSchedulerID::POLLING_UPDATE);
    } else {
      this->disable_loop();
    }
  }

#ifndef USE_PREFERENCES_SYNC_EVERY_LOOP
 protected:
  uint32_t write_interval_{60000};
#endif
};

}  // namespace preferences
}  // namespace esphome
