#pragma once

#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/gps/gps.h"

namespace esphome {
namespace gps {

class GPSTime : public time::RealTimeClock, public GPSListener {
 public:
  void on_update(TinyGPSPlus &tiny_gps) override {
    if (!this->has_time_)
      this->from_tiny_gps_(tiny_gps);
  }
  void setup() override {
    this->set_interval(5 * 60 * 1000, [this]() { this->from_tiny_gps_(this->get_tiny_gps()); });
  }

 protected:
  void from_tiny_gps_(TinyGPSPlus &tiny_gps) {
    if (!tiny_gps.time.isValid() || !tiny_gps.date.isValid())
      return;
    time::ESPTime val{};
    val.year = tiny_gps.date.year();
    val.month = tiny_gps.date.month();
    val.day_of_month = tiny_gps.date.day();
    val.hour = tiny_gps.time.hour();
    val.minute = tiny_gps.time.minute();
    val.second = tiny_gps.time.second();
    val.recalc_timestamp_utc(false);
    this->synchronize_epoch_(val.timestamp);
    this->has_time_ = true;
  }
  bool has_time_{false};
};

}  // namespace gps
}  // namespace esphome
