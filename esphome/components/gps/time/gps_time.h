#pragma once

#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/gps/gps.h"

namespace esphome {
namespace gps {

class GPSTime : public time::RealTimeClock, public GPSListener {
 public:
  void on_update(TinyGPSPlus &tiny_gps) override {
    if (!tiny_gps.time.isValid() || !tiny_gps.date.isValid()) {
      return;
    }
    time::ESPTime val{};
    val.year = tiny_gps.date.year();
    val.month = tiny_gps.date.month();
    val.day_of_month = tiny_gps.date.day();
    val.hour = tiny_gps.time.hour();
    val.minute = tiny_gps.time.minute();
    val.second = tiny_gps.time.second();
    this->synchronize_epoch_();
  }
};

}  // namespace gps
}  // namespace esphome
