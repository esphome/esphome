#include "fan.h"
#include "esphome/core/log.h"

namespace esphome {
namespace fan {

static const char *TAG = "fan";

const FanTraits &Fan::get_traits() const { return this->traits_; }
void Fan::set_traits(const FanTraits &traits) { this->traits_ = traits; }
void Fan::add_on_state_callback(std::function<void()> &&callback) { this->state_callback_.add(std::move(callback)); }
Fan::Fan(const std::string &name) : Nameable(name) {}

FanCall Fan::turn_on() { return this->make_call().set_state(true); }
FanCall Fan::turn_off() { return this->make_call().set_state(false); }
FanCall Fan::toggle() { return this->make_call().set_state(!this->state); }
FanCall Fan::make_call() { return FanCall(this); }

struct FanRTCState {
  bool state;
  FanSpeed speed;
  bool oscillating;
  FanDirection direction;
};

void Fan::restore_state_() {
  this->rtc_ = global_preferences.make_preference<FanRTCState>(this->get_object_id_hash());
  FanRTCState recovered{};
  if (!this->rtc_.load(&recovered))
    return;

  auto call = this->make_call();
  call.set_state(recovered.state);
  call.set_speed(recovered.speed);
  call.set_oscillating(recovered.oscillating);
  call.set_direction(recovered.direction);
  call.perform();
}
uint32_t Fan::hash_base() { return 418001110UL; }

void Fan::publish_state() {
  this->state_callback_.call();

  FanRTCState saved{};
  saved.state = this->state;
  saved.speed = this->speed;
  saved.oscillating = this->oscillating;
  saved.direction = this->direction;
  this->rtc_.save(&saved);
}

void FanCall::perform() const {
  if (this->state_.has_value()) {
    this->parent_->state = *this->state_;
  }
  if (this->oscillating_.has_value()) {
    this->parent_->oscillating = *this->oscillating_;
  }
  if (this->direction_.has_value()) {
    this->parent_->direction = *this->direction_;
  }
  if (this->speed_.has_value()) {
    switch (*this->speed_) {
      case FAN_SPEED_LOW:
      case FAN_SPEED_MEDIUM:
      case FAN_SPEED_HIGH:
        this->parent_->speed = *this->speed_;
        break;
      default:
        // protect from invalid input
        break;
    }
  }
  this->parent_->control();
}
FanCall &FanCall::set_speed(const char *speed) {
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
