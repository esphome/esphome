#pragma once

#include "esphome/core/preferences.h"
#include "esphome/core/component.h"

namespace esphome {
namespace preferences {

class IntervalSyncer : public Component {
 public:
  void set_write_interval(uint32_t write_interval) { write_interval_ = write_interval; }
  void setup() override {
    set_interval(write_interval_, []() { global_preferences->sync(); });
  }
  void on_shutdown() override { global_preferences->sync(); }
  float get_setup_priority() const override { return setup_priority::BUS; }

 protected:
  uint32_t write_interval_;
};

}  // namespace preferences
}  // namespace esphome
