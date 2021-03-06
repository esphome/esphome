#include "fan_state.h"
#include "fan_helpers.h"
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

FanStateCall FanState::turn_on() { return this->make_call().set_state(true); }
FanStateCall FanState::turn_off() { return this->make_call().set_state(false); }
FanStateCall FanState::toggle() { return this->make_call().set_state(!this->state); }
FanStateCall FanState::make_call() { return FanStateCall(this); }

struct FanStateRTCState {
  bool state;
  int speed;
  bool oscillating;
  FanDirection direction;
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
  call.set_direction(recovered.direction);
  call.perform();
}
float FanState::get_setup_priority() const { return setup_priority::HARDWARE - 1.0f; }
uint32_t FanState::hash_base() { return 418001110UL; }

void FanStateCall::perform() const {
  if (this->binary_state_.has_value()) {
    this->state_->state = *this->binary_state_;
  }
  if (this->oscillating_.has_value()) {
    this->state_->oscillating = *this->oscillating_;
  }
  if (this->direction_.has_value()) {
    this->state_->direction = *this->direction_;
  }
  if (this->speed_level_.has_value()) {
    const int speed_levels = this->state_->get_traits().supported_speed_levels();
    this->state_->speed_level = clamp(*this->speed_level_, 1, speed_levels);
  }

  FanStateRTCState saved{};
  saved.state = this->state_->state;
  saved.speed = this->state_->speed_level;
  saved.oscillating = this->state_->oscillating;
  saved.direction = this->state_->direction;
  this->state_->rtc_.save(&saved);

  this->state_->state_callback_.call();
}

}  // namespace fan
}  // namespace esphome
