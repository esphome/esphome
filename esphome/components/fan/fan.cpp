#include "fan.h"
#include "fan_helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace fan {

static const char *const TAG = "fan";

const LogString *fan_direction_to_string(FanDirection direction) {
  switch (direction) {
    case FAN_DIRECTION_FORWARD:
      return LOG_STR("FORWARD");
    case FAN_DIRECTION_REVERSE:
      return LOG_STR("REVERSE");
    default:
      return LOG_STR("UNKNOWN");
  }
}

void FanCall::perform() {
  ESP_LOGD(TAG, "'%s' - Setting:", this->parent_.get_name().c_str());
  this->validate_();
  if (this->binary_state_.has_value())
    ESP_LOGD(TAG, "  State: %s", ONOFF(*this->binary_state_));
  if (this->oscillating_.has_value())
    ESP_LOGD(TAG, "  Oscillating: %s", YESNO(*this->oscillating_));
  if (this->speed_.has_value())
    ESP_LOGD(TAG, "  Speed: %d", *this->speed_);
  if (this->direction_.has_value())
    ESP_LOGD(TAG, "  Direction: %s", *this->direction_ == FAN_DIRECTION_FORWARD ? "Forward" : "Reverse");

  this->parent_.control(*this);
}
void FanCall::validate_() {
  auto traits = this->parent_.get_traits();

  if (this->speed_.has_value())
    this->speed_ = clamp(*this->speed_, 1, traits.supported_speed_count());
}

// This whole method is deprecated, don't warn about usage of deprecated methods inside of it.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
FanCall &FanCall::set_speed(const char *legacy_speed) {
  const auto supported_speed_count = this->parent_.get_traits().supported_speed_count();
  if (strcasecmp(legacy_speed, "low") == 0) {
    this->set_speed(fan::speed_enum_to_level(FAN_SPEED_LOW, supported_speed_count));
  } else if (strcasecmp(legacy_speed, "medium") == 0) {
    this->set_speed(fan::speed_enum_to_level(FAN_SPEED_MEDIUM, supported_speed_count));
  } else if (strcasecmp(legacy_speed, "high") == 0) {
    this->set_speed(fan::speed_enum_to_level(FAN_SPEED_HIGH, supported_speed_count));
  }
  return *this;
}
#pragma GCC diagnostic pop

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

Fan::Fan() : EntityBase("") {}
Fan::Fan(const std::string &name) : EntityBase(name) {}

FanCall Fan::turn_on() { return this->make_call().set_state(true); }
FanCall Fan::turn_off() { return this->make_call().set_state(false); }
FanCall Fan::toggle() { return this->make_call().set_state(!this->state); }
FanCall Fan::make_call() { return FanCall(*this); }

void Fan::add_on_state_callback(std::function<void()> &&callback) { this->state_callback_.add(std::move(callback)); }
void Fan::publish_state() {
  ESP_LOGD(TAG, "'%s' - Sending state:", this->name_.c_str());
  // TODO

  this->save_state_();
  this->state_callback_.call();
}

// Random 32-bit value, change this every time the layout of the FanRestoreState struct changes.
constexpr uint32_t RESTORE_STATE_VERSION = 0x00000000;
optional<FanRestoreState> Fan::restore_state_() {
  this->rtc_ = global_preferences->make_preference<FanRestoreState>(this->get_object_id_hash() ^ RESTORE_STATE_VERSION);
  FanRestoreState recovered{};
  if (!this->rtc_.load(&recovered))
    return {};
  return recovered;
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
  ESP_LOGCONFIG(tag, "Fan '%s':", this->get_name().c_str());
  if (this->get_traits().supports_speed()) {
    ESP_LOGCONFIG(tag, "%s  Speed: YES", prefix);
    ESP_LOGCONFIG(tag, "%s  Speed count: %d", prefix, this->get_traits().supported_speed_count());
  }
  if (this->get_traits().supports_oscillation())
    ESP_LOGCONFIG(tag, "%s  Oscillation: YES", prefix);
  if (this->get_traits().supports_direction())
    ESP_LOGCONFIG(tag, "%s  Direction: YES", prefix);
}
uint32_t Fan::hash_base() { return 418001110UL; }

}  // namespace fan
}  // namespace esphome
