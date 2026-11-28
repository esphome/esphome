#pragma once

#include "esphome/components/gps/gps.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/component.h"

namespace esphome::gps {

class GPSTime final : public time::RealTimeClock, public GPSListener {
 public:
  void update() override { this->sync_pending_ = true; }
  void on_update(const GPSData &data) override;

 protected:
  bool sync_pending_{true};
};

}  // namespace esphome::gps
