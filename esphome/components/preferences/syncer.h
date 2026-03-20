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
    this->set_interval(InternalSchedulerID::POLLING_UPDATE, this->write_interval_,
                       []() { global_preferences->sync(); });
  }
  /// Resume periodic preference syncing.
  void start_poller() {
    this->set_interval(InternalSchedulerID::POLLING_UPDATE, this->write_interval_,
                       []() { global_preferences->sync(); });
  }
  /// Suspend periodic preference syncing.
  void stop_poller() { this->cancel_interval(InternalSchedulerID::POLLING_UPDATE); }
#endif
  void on_shutdown() override { global_preferences->sync(); }
  float get_setup_priority() const override { return setup_priority::BUS; }

#ifndef USE_PREFERENCES_SYNC_EVERY_LOOP
 protected:
  uint32_t write_interval_{60000};
#endif
};

}  // namespace preferences
}  // namespace esphome
