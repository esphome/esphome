#include "template_real_time_clock.h"

namespace esphome::template_ {

static constexpr int64_t MIN_VALID_EPOCH = 1546300800;  // January 1, 2019

void TemplateRealTimeClock::update() {
  if (!this->sync_system_time_)
    return;
  auto val = this->f_.call();
  if (!val.has_value() || *val < MIN_VALID_EPOCH)
    return;
  this->synchronize_epoch_(static_cast<uint32_t>(*val));
}

time_t TemplateRealTimeClock::timestamp_now() {
  auto val = this->f_.call();
  if (val.has_value()) {
    return static_cast<time_t>(*val);
  }
  return 0;
}

}  // namespace esphome::template_
