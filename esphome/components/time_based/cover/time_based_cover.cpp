#include "time_based_cover.h"
#include "esphome/core/log.h"

namespace esphome::time_based {

static const char *const TAG = "time_based.cover";

using namespace esphome::cover;

void TimeBasedCover::dump_config() {
  LOG_COVER("", "Time Based Cover", this);
  ESP_LOGCONFIG(TAG,
                "  Open Duration: %.1fs\n"
                "  Close Duration: %.1fs",
                this->open_duration_ / 1e3f, this->close_duration_ / 1e3f);
}

CoverTraits TimeBasedCover::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_stop(true);
  traits.set_supports_position(true);
  traits.set_supports_toggle(true);
  traits.set_is_assumed_state(this->get_assumed_state());
  return traits;
}

}  // namespace esphome::time_based
