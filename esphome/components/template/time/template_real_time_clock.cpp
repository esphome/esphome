#include "template_real_time_clock.h"

namespace esphome::template_ {

static const char *const TAG = "template.time";

time_t TemplateRealTimeClock::timestamp_now() {
  auto val = this->f_.call();
  if (val.has_value()) {
    auto value = *val;
    if (value < 0 || value > std::numeric_limits<time_t>::max()) {
      ESP_LOGW(TAG, "timestamp value out of range: %lld", static_cast<long long>(value));
      return 0;
    }
    return static_cast<time_t>(value);
  }
  return 0;
}

}  // namespace esphome::template_
