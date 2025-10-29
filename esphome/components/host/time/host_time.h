#pragma once

#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace host {

class HostTime : public time::RealTimeClock {
 public:
  void update() override {}
};

}  // namespace host
}  // namespace esphome
