#include "template_real_time_clock.h"

namespace esphome::template_ {

time_t TemplateRealTimeClock::timestamp_now() {
  auto val = this->f_.call();
  if (val.has_value()) {
    return static_cast<time_t>(*val);
  }
  return 0;
}

}  // namespace esphome::template_
