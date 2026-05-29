#include "feedback_valve.h"
#include "esphome/core/log.h"

namespace esphome::feedback {

static const char *const TAG = "feedback.valve";

using namespace esphome::valve;

ValveTraits FeedbackValve::get_traits() {
  auto traits = ValveTraits();
  traits.set_supports_stop(true);
  traits.set_supports_position(true);
  traits.set_supports_toggle(true);
  traits.set_is_assumed_state(this->get_assumed_state());
  return traits;
}

void FeedbackValve::dump_config() {
  LOG_VALVE("", "Feedback Valve", this);
  ESP_LOGCONFIG(TAG, "  Open Duration: %.1fs", this->get_open_duration() / 1e3f);
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Open Endstop", this->get_open_endstop());
  LOG_BINARY_SENSOR("  ", "Open Feedback", this->get_open_feedback());
  LOG_BINARY_SENSOR("  ", "Open Obstacle", this->get_open_obstacle());
#endif
  ESP_LOGCONFIG(TAG, "  Close Duration: %.1fs", this->get_close_duration() / 1e3f);
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Close Endstop", this->get_close_endstop());
  LOG_BINARY_SENSOR("  ", "Close Feedback", this->get_close_feedback());
  LOG_BINARY_SENSOR("  ", "Close Obstacle", this->get_close_obstacle());
#endif
  if (this->get_has_built_in_endstop()) {
    ESP_LOGCONFIG(TAG, "  Has builtin endstop: YES");
  }
  if (this->get_infer_endstop()) {
    ESP_LOGCONFIG(TAG, "  Infer endstop from movement: YES");
  }
  if (this->get_max_duration() < UINT32_MAX) {
    ESP_LOGCONFIG(TAG, "  Max Duration: %.1fs", this->get_max_duration() / 1e3f);
  }
  if (this->get_direction_change_waittime().has_value()) {
    ESP_LOGCONFIG(TAG, "  Direction change wait time: %.1fs", *this->get_direction_change_waittime() / 1e3f);
  }
  if (this->get_acceleration_wait_time()) {
    ESP_LOGCONFIG(TAG, "  Acceleration wait time: %.1fs", this->get_acceleration_wait_time() / 1e3f);
  }
#ifdef USE_BINARY_SENSOR
  if (this->get_obstacle_rollback() &&
      (this->get_open_obstacle() != nullptr || this->get_close_obstacle() != nullptr)) {
    ESP_LOGCONFIG(TAG, "  Obstacle rollback: %.1f%%", this->get_obstacle_rollback() * 100);
  }
#endif
}

}  // namespace esphome::feedback
