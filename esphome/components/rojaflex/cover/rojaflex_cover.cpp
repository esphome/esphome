#include "rojaflex_cover.h"

#include "esphome/core/log.h"

namespace esphome::rojaflex {

static const char *const TAG = "rojaflex.cover";

void RojaflexCover::setup() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "No parent configured");
    this->mark_failed();
    return;
  }
  this->parent_->register_cover(this, this->channel_);
  this->sync_from_parent();
}

void RojaflexCover::dump_config() {
  LOG_COVER("", "Rojaflex Cover", this);
  ESP_LOGCONFIG(TAG, "  Channel: %u", static_cast<unsigned>(this->channel_));
  ESP_LOGCONFIG(TAG, "  Status: %s", this->parent_->get_channel_status(this->channel_).c_str());
}

cover::CoverTraits RojaflexCover::get_traits() {
  auto traits = cover::CoverTraits();
  traits.set_supports_stop(true);
  traits.set_supports_position(true);
  traits.set_is_assumed_state(false);
  return traits;
}

void RojaflexCover::control(const cover::CoverCall &call) {
  if (this->parent_ == nullptr) {
    return;
  }
  if (call.get_stop()) {
    this->parent_->send_command(this->channel_, static_cast<uint8_t>(Command::STOP));
  }
  if (auto pos = call.get_position(); pos.has_value()) {
    const int target_pct = static_cast<int>((1.0f - *pos) * 100.0f + 0.5f);
    this->parent_->set_position(this->channel_, target_pct);
  }
}

void RojaflexCover::sync_from_parent() {
  if (this->parent_ == nullptr) {
    return;
  }
  const int pct = this->parent_->get_motor_pct(this->channel_);
  if (pct < 0) {
    return;
  }
  const int clamped = pct > 100 ? 100 : pct;
  this->position = 1.0f - clamped / 100.0f;
  this->current_operation = cover::COVER_OPERATION_IDLE;
  this->publish_state();
}

}  // namespace esphome::rojaflex
