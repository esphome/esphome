#include "template_real_time_clock.h"

namespace esphome::template_ {

void TemplateRealTimeClock::update() {
  if (this->sync_system_time_) {
    this->synchronize_epoch_(static_cast<uint32_t>(this->timestamp_now()));
  } else {
    this->stop_poller();
  }
}

time_t TemplateRealTimeClock::timestamp_now() {
  auto val = this->f_.call();
  if (val.has_value()) {
    return static_cast<time_t>(*val);
  }
  return 0;
}

}  // namespace esphome::template_
