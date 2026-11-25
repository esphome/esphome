#pragma once

#include "esphome/components/gnss/gnss.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome::gnss {

/// Sets the clock from the first solution with a time, then on each update_interval.
class GNSSTime final : public time::RealTimeClock {
 public:
  explicit GNSSTime(GNSSComponent *parent) : parent_(parent) {}
  void update() override { this->sync_(this->parent_->get_data()); }
  void on_fix(const GNSSData &data) {
    if (!this->has_time_)
      this->sync_(data);
  }

 protected:
  void sync_(const GNSSData &data);

  GNSSComponent *parent_;
  bool has_time_{false};
};

}  // namespace esphome::gnss
