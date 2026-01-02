#pragma once

#include "esphome/components/gps/gps.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/component.h"

namespace esphome {
namespace gps {

class GPSTime : public time::RealTimeClock, public GPSListener {
 public:
  void update() override { this->from_tiny_gps_(this->get_tiny_gps()); };
  void on_update(TinyGPSPlus &tiny_gps) override {
    if (!this->has_time_) {
      this->from_tiny_gps_(tiny_gps);
    }
  }

 protected:
  void from_tiny_gps_(TinyGPSPlus &tiny_gps);
  bool has_time_{false};
};

}  // namespace gps
}  // namespace esphome
