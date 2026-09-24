#include "endstop_valve.h"
#include "esphome/core/log.h"

namespace esphome::endstop {

static const char *const TAG = "endstop.valve";

using namespace esphome::valve;

ValveTraits EndstopValve::get_traits() {
  auto traits = ValveTraits();
  traits.set_supports_stop(true);
  traits.set_supports_position(true);
  traits.set_supports_toggle(true);
  traits.set_is_assumed_state(false);
  return traits;
}

void EndstopValve::dump_config() {
  LOG_VALVE("", "Endstop Valve", this);
  ESP_LOGCONFIG(TAG,
                "  Open Duration: %.1fs\n"
                "  Close Duration: %.1fs",
                this->open_duration_ / 1e3f, this->close_duration_ / 1e3f);
  LOG_BINARY_SENSOR("  ", "Open Endstop", this->open_endstop_);
  LOG_BINARY_SENSOR("  ", "Close Endstop", this->close_endstop_);
}

}  // namespace esphome::endstop
