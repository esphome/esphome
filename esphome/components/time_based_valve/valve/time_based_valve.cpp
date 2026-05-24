#include "time_based_valve.h"
#include "esphome/core/log.h"

namespace esphome::time_based_valve {

static const char *const TAG = "time_based_valve";

using namespace esphome::valve;

void TimeBasedValve::dump_config() {
  LOG_VALVE("", "Time Based Valve", this);
  ESP_LOGCONFIG(TAG,
                "  Open Duration: %.1fs\n"
                "  Close Duration: %.1fs",
                this->open_duration_ / 1e3f, this->close_duration_ / 1e3f);
}

void TimeBasedValve::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    this->position = 0.5f;
    this->publish_state();
  }
}

ValveTraits TimeBasedValve::get_traits() {
  auto traits = ValveTraits();
  traits.set_supports_stop(true);
  traits.set_supports_position(true);
  traits.set_supports_toggle(true);
  traits.set_is_assumed_state(this->get_assumed_state());
  return traits;
}

}  // namespace esphome::time_based_valve
