#include "tilt_cover.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tilt {

static const char *TAG = "tilt.cover";

using namespace esphome::cover;

void TiltCover::dump_config() {
  LOG_COVER("", "Tilt Cover", this);
  ESP_LOGCONFIG(TAG, "  Tilt Closed Value: %.0f%%", this->get_tilt_closed_value() * 100.0);
  ESP_LOGCONFIG(TAG, "  Tilt Opened Value: %.0f%%", this->get_tilt_opened_value() * 100.0);
}
void TiltCover::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    this->tilt = 0.5f;
  }
}
float TiltCover::get_setup_priority() const { return setup_priority::DATA; }
CoverTraits TiltCover::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_position(false);
  traits.set_supports_tilt(true);
  traits.set_is_assumed_state(false);
  return traits;
}
void TiltCover::control(const CoverCall &call) {
  if (call.get_tilt().has_value()) {
    this->tilt = *call.get_tilt();
    this->publish_state();
    Trigger<> *trig;
    trig = this->tilt_trigger_;
    trig->trigger();
  }
}

}  // namespace tilt
}  // namespace esphome
