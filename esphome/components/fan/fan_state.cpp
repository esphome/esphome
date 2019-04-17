#include "fan_state.h"
#include "esphome/core/log.h"

namespace esphome {
namespace fan {

static const char *TAG = "fan";

const FanTraits &FanState::get_traits() const { return this->traits_; }
void FanState::set_traits(const FanTraits &traits) { this->traits_ = traits; }
void FanState::add_on_state_callback(std::function<void()> &&callback) {
  this->state_callback_.add(std::move(callback));
}
FanState::FanState(const std::string &name) : Nameable(name) {}

FanState::StateCall FanState::turn_on() { return this->make_call().set_state(true); }
FanState::StateCall FanState::turn_off() { return this->make_call().set_state(false); }
FanState::StateCall FanState::toggle() { return this->make_call().set_state(!this->state); }
FanState::StateCall FanState::make_call() { return FanState::StateCall(this); }

struct FanStateRTCState {
  bool state;
  FanSpeed speed;
  bool oscillating;
};

void FanState::setup() {
  this->rtc_ = global_preferences.make_preference<FanStateRTCState>(this->get_object_id_hash());
  FanStateRTCState recovered{};
  if (!this->rtc_.load(&recovered))
    return;

  auto call = this->make_call();
  call.set_state(recovered.state);
  call.set_speed(recovered.speed);
  call.set_oscillating(recovered.oscillating);
  call.perform();
}
float FanState::get_setup_priority() const { return setup_priority::HARDWARE - 1.0f; }
uint32_t FanState::hash_base() { return 418001110UL; }

FanState::StateCall::StateCall(FanState *state) : state_(state) {}
FanState::StateCall &FanState::StateCall::set_state(bool state) {
  this->binary_state_ = state;
  return *this;
}
FanState::StateCall &FanState::StateCall::set_state(optional<bool> state) {
  this->binary_state_ = state;
  return *this;
}
FanState::StateCall &FanState::StateCall::set_oscillating(bool oscillating) {
  this->oscillating_ = oscillating;
  return *this;
}
FanState::StateCall &FanState::StateCall::set_oscillating(optional<bool> oscillating) {
  this->oscillating_ = oscillating;
  return *this;
}
FanState::StateCall &FanState::StateCall::set_speed(FanSpeed speed) {
  this->speed_ = speed;
  return *this;
}
FanState::StateCall &FanState::StateCall::set_speed(optional<FanSpeed> speed) {
  this->speed_ = speed;
  return *this;
}
void FanState::StateCall::perform() const {
  if (this->binary_state_.has_value()) {
    this->state_->state = *this->binary_state_;
  }
  if (this->oscillating_.has_value()) {
    this->state_->oscillating = *this->oscillating_;
  }
  if (this->speed_.has_value()) {
    switch (*this->speed_) {
      case FAN_SPEED_LOW:
      case FAN_SPEED_MEDIUM:
      case FAN_SPEED_HIGH:
        this->state_->speed = *this->speed_;
        break;
      default:
        // protect from invalid input
        break;
    }
  }

  FanStateRTCState saved{};
  saved.state = this->state_->state;
  saved.speed = this->state_->speed;
  saved.oscillating = this->state_->oscillating;
  this->state_->rtc_.save(&saved);

  this->state_->state_callback_.call();
}
FanState::StateCall &FanState::StateCall::set_speed(const char *speed) {
  if (strcasecmp(speed, "low") == 0) {
    this->set_speed(FAN_SPEED_LOW);
  } else if (strcasecmp(speed, "medium") == 0) {
    this->set_speed(FAN_SPEED_MEDIUM);
  } else if (strcasecmp(speed, "high") == 0) {
    this->set_speed(FAN_SPEED_HIGH);
  }
  return *this;
}

}  // namespace fan
}  // namespace esphome
