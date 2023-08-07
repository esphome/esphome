#include "fan.h"
#include "esphome/core/log.h"

namespace esphome {
namespace fan {

static const char *const TAG = "fan";

const LogString *fan_direction_to_string(FanDirection direction) {
  switch (direction) {
    case FanDirection::FORWARD:
      return LOG_STR("FORWARD");
    case FanDirection::REVERSE:
      return LOG_STR("REVERSE");
    default:
      return LOG_STR("UNKNOWN");
  }
}

void FanCall::perform() {
  ESP_LOGD(TAG, "'%s' - Setting:", this->parent_.get_name().c_str());
  this->validate_();
  if (this->binary_state_.has_value()) {
    ESP_LOGD(TAG, "  State: %s", ONOFF(*this->binary_state_));
  }
  if (this->oscillating_.has_value()) {
    ESP_LOGD(TAG, "  Oscillating: %s", YESNO(*this->oscillating_));
  }
  if (this->speed_.has_value()) {
    ESP_LOGD(TAG, "  Speed: %d", *this->speed_);
  }
  if (this->direction_.has_value()) {
    ESP_LOGD(TAG, "  Direction: %s", LOG_STR_ARG(fan_direction_to_string(*this->direction_)));
  }

  this->parent_.control(*this);
}
void FanCall::validate_() {
  auto traits = this->parent_.get_traits();

  if (this->speed_.has_value())
    this->speed_ = clamp(*this->speed_, 1, traits.supported_speed_count());

  if (this->binary_state_.has_value() && *this->binary_state_) {
    // when turning on, if current speed is zero, set speed to 100%
    if (traits.supports_speed() && !this->parent_.state && this->parent_.speed == 0) {
      this->speed_ = traits.supported_speed_count();
    }
  }

  if (this->oscillating_.has_value() && !traits.supports_oscillation()) {
    ESP_LOGW(TAG, "'%s' - This fan does not support oscillation!", this->parent_.get_name().c_str());
    this->oscillating_.reset();
  }

  if (this->speed_.has_value() && !traits.supports_speed()) {
    ESP_LOGW(TAG, "'%s' - This fan does not support speeds!", this->parent_.get_name().c_str());
    this->speed_.reset();
  }

  if (this->direction_.has_value() && !traits.supports_direction()) {
    ESP_LOGW(TAG, "'%s' - This fan does not support directions!", this->parent_.get_name().c_str());
    this->direction_.reset();
  }
}

FanCall FanRestoreState::to_call(Fan &fan) {
  auto call = fan.make_call();
  call.set_state(this->state);
  call.set_oscillating(this->oscillating);
  call.set_speed(this->speed);
  call.set_direction(this->direction);
  return call;
}
void FanRestoreState::apply(Fan &fan) {
  fan.state = this->state;
  fan.oscillating = this->oscillating;
  fan.speed = this->speed;
  fan.direction = this->direction;
  fan.publish_state();
}

FanCall Fan::turn_on() { return this->make_call().set_state(true); }
FanCall Fan::turn_off() { return this->make_call().set_state(false); }
FanCall Fan::toggle() { return this->make_call().set_state(!this->state); }
FanCall Fan::make_call() { return FanCall(*this); }

void Fan::add_on_state_callback(std::function<void()> &&callback) { this->state_callback_.add(std::move(callback)); }
void Fan::publish_state() {
  auto traits = this->get_traits();

  ESP_LOGD(TAG, "'%s' - Sending state:", this->name_.c_str());
  ESP_LOGD(TAG, "  State: %s", ONOFF(this->state));
  if (traits.supports_speed()) {
    ESP_LOGD(TAG, "  Speed: %d", this->speed);
  }
  if (traits.supports_oscillation()) {
    ESP_LOGD(TAG, "  Oscillating: %s", YESNO(this->oscillating));
  }
  if (traits.supports_direction()) {
    ESP_LOGD(TAG, "  Direction: %s", LOG_STR_ARG(fan_direction_to_string(this->direction)));
  }

  this->state_callback_.call();
  this->save_state_();
}

// Random 32-bit value, change this every time the layout of the FanRestoreState struct changes.
constexpr uint32_t RESTORE_STATE_VERSION = 0x71700ABA;
optional<FanRestoreState> Fan::restore_state_() {
  FanRestoreState recovered{};
  this->rtc_ = global_preferences->make_preference<FanRestoreState>(this->get_object_id_hash() ^ RESTORE_STATE_VERSION);
  bool restored = this->rtc_.load(&recovered);

  switch (this->restore_mode_) {
    case FanRestoreMode::NO_RESTORE:
      return {};
    case FanRestoreMode::ALWAYS_OFF:
      recovered.state = false;
      return recovered;
    case FanRestoreMode::ALWAYS_ON:
      recovered.state = true;
      return recovered;
    case FanRestoreMode::RESTORE_DEFAULT_OFF:
      recovered.state = restored ? recovered.state : false;
      return recovered;
    case FanRestoreMode::RESTORE_DEFAULT_ON:
      recovered.state = restored ? recovered.state : true;
      return recovered;
    case FanRestoreMode::RESTORE_INVERTED_DEFAULT_OFF:
      recovered.state = restored ? !recovered.state : false;
      return recovered;
    case FanRestoreMode::RESTORE_INVERTED_DEFAULT_ON:
      recovered.state = restored ? !recovered.state : true;
      return recovered;
  }

  return {};
}
void Fan::save_state_() {
  FanRestoreState state{};
  state.state = this->state;
  state.oscillating = this->oscillating;
  state.speed = this->speed;
  state.direction = this->direction;
  this->rtc_.save(&state);
}

void Fan::dump_traits_(const char *tag, const char *prefix) {
  if (this->get_traits().supports_speed()) {
    ESP_LOGCONFIG(tag, "%s  Speed: YES", prefix);
    ESP_LOGCONFIG(tag, "%s  Speed count: %d", prefix, this->get_traits().supported_speed_count());
  }
  if (this->get_traits().supports_oscillation()) {
    ESP_LOGCONFIG(tag, "%s  Oscillation: YES", prefix);
  }
  if (this->get_traits().supports_direction()) {
    ESP_LOGCONFIG(tag, "%s  Direction: YES", prefix);
  }
}

}  // namespace fan
}  // namespace esphome
