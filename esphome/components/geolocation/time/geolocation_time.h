#pragma once

#include "esphome/components/geolocation/geolocation.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome::geolocation {

/// Sets the clock from the first solution with a time, then on each update_interval.
class GeolocationTime final : public time::RealTimeClock {
 public:
  explicit GeolocationTime(GeolocationComponent *parent) : parent_(parent) {}
  void update() override { this->sync_(this->parent_->get_data()); }
  void on_fix(const GeolocationData &data) {
    if (!this->has_time_)
      this->sync_(data);
  }

 protected:
  void sync_(const GeolocationData &data);

  GeolocationComponent *parent_;
  bool has_time_{false};
};

}  // namespace esphome::geolocation
