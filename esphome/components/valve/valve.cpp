#include "valve.h"
#include "esphome/core/log.h"

namespace esphome {
namespace valve {

static const char *const TAG = "valve";

const float VALVE_OPEN = 1.0f;
const float VALVE_CLOSED = 0.0f;

const char *valve_command_to_str(float pos) {
  if (pos == VALVE_OPEN) {
    return "OPEN";
  } else if (pos == VALVE_CLOSED) {
    return "CLOSE";
  } else {
    return "UNKNOWN";
  }
}
const char *valve_operation_to_str(ValveOperation op) {
  switch (op) {
    case VALVE_OPERATION_IDLE:
      return "IDLE";
    case VALVE_OPERATION_OPENING:
      return "OPENING";
    case VALVE_OPERATION_CLOSING:
      return "CLOSING";
    default:
      return "UNKNOWN";
  }
}

Valve::Valve() : position{VALVE_OPEN} {}

ValveCall::ValveCall(Valve *parent) : parent_(parent) {}
ValveCall &ValveCall::set_command(const char *command) {
  if (strcasecmp(command, "OPEN") == 0) {
    this->set_command_open();
  } else if (strcasecmp(command, "CLOSE") == 0) {
    this->set_command_close();
  } else if (strcasecmp(command, "STOP") == 0) {
    this->set_command_stop();
  } else if (strcasecmp(command, "TOGGLE") == 0) {
    this->set_command_toggle();
  } else {
    ESP_LOGW(TAG, "'%s' - Unrecognized command %s", this->parent_->get_name().c_str(), command);
  }
  return *this;
}
ValveCall &ValveCall::set_command_open() {
  this->position_ = VALVE_OPEN;
  return *this;
}
ValveCall &ValveCall::set_command_close() {
  this->position_ = VALVE_CLOSED;
  return *this;
}
ValveCall &ValveCall::set_command_stop() {
  this->stop_ = true;
  return *this;
}
ValveCall &ValveCall::set_command_toggle() {
  this->toggle_ = true;
  return *this;
}
ValveCall &ValveCall::set_position(float position) {
  this->position_ = position;
  return *this;
}
void ValveCall::perform() {
  ESP_LOGD(TAG, "'%s' - Setting", this->parent_->get_name().c_str());
  auto traits = this->parent_->get_traits();
  this->validate_();
  if (this->stop_) {
    ESP_LOGD(TAG, "  Command: STOP");
  }
  if (this->position_.has_value()) {
    if (traits.get_supports_position()) {
      ESP_LOGD(TAG, "  Position: %.0f%%", *this->position_ * 100.0f);
    } else {
      ESP_LOGD(TAG, "  Command: %s", valve_command_to_str(*this->position_));
    }
  }
  if (this->toggle_.has_value()) {
    ESP_LOGD(TAG, "  Command: TOGGLE");
  }
  this->parent_->control(*this);
}
const optional<float> &ValveCall::get_position() const { return this->position_; }
const optional<bool> &ValveCall::get_toggle() const { return this->toggle_; }
void ValveCall::validate_() {
  auto traits = this->parent_->get_traits();
  if (this->position_.has_value()) {
    auto pos = *this->position_;
    if (!traits.get_supports_position() && pos != VALVE_OPEN && pos != VALVE_CLOSED) {
      ESP_LOGW(TAG, "'%s' - This valve device does not support setting position!", this->parent_->get_name().c_str());
      this->position_.reset();
    } else if (pos < 0.0f || pos > 1.0f) {
      ESP_LOGW(TAG, "'%s' - Position %.2f is out of range [0.0 - 1.0]", this->parent_->get_name().c_str(), pos);
      this->position_ = clamp(pos, 0.0f, 1.0f);
    }
  }
  if (this->toggle_.has_value()) {
    if (!traits.get_supports_toggle()) {
      ESP_LOGW(TAG, "'%s' - This valve device does not support toggle!", this->parent_->get_name().c_str());
      this->toggle_.reset();
    }
  }
  if (this->stop_) {
    if (this->position_.has_value()) {
      ESP_LOGW(TAG, "Cannot set position when stopping a valve!");
      this->position_.reset();
    }
    if (this->toggle_.has_value()) {
      ESP_LOGW(TAG, "Cannot set toggle when stopping a valve!");
      this->toggle_.reset();
    }
  }
}
ValveCall &ValveCall::set_stop(bool stop) {
  this->stop_ = stop;
  return *this;
}
bool ValveCall::get_stop() const { return this->stop_; }

ValveCall Valve::make_call() { return {this}; }

void Valve::add_on_state_callback(std::function<void()> &&f) { this->state_callback_.add(std::move(f)); }
void Valve::publish_state(bool save) {
  this->position = clamp(this->position, 0.0f, 1.0f);

  ESP_LOGD(TAG, "'%s' - Publishing:", this->name_.c_str());
  auto traits = this->get_traits();
  if (traits.get_supports_position()) {
    ESP_LOGD(TAG, "  Position: %.0f%%", this->position * 100.0f);
  } else {
    if (this->position == VALVE_OPEN) {
      ESP_LOGD(TAG, "  State: OPEN");
    } else if (this->position == VALVE_CLOSED) {
      ESP_LOGD(TAG, "  State: CLOSED");
    } else {
      ESP_LOGD(TAG, "  State: UNKNOWN");
    }
  }
  ESP_LOGD(TAG, "  Current Operation: %s", valve_operation_to_str(this->current_operation));

  this->state_callback_.call();

  if (save) {
    ValveRestoreState restore{};
    memset(&restore, 0, sizeof(restore));
    restore.position = this->position;
    this->rtc_.save(&restore);
  }
}
optional<ValveRestoreState> Valve::restore_state_() {
  this->rtc_ = global_preferences->make_preference<ValveRestoreState>(this->get_object_id_hash());
  ValveRestoreState recovered{};
  if (!this->rtc_.load(&recovered))
    return {};
  return recovered;
}

bool Valve::is_fully_open() const { return this->position == VALVE_OPEN; }
bool Valve::is_fully_closed() const { return this->position == VALVE_CLOSED; }

ValveCall ValveRestoreState::to_call(Valve *valve) {
  auto call = valve->make_call();
  call.set_position(this->position);
  return call;
}
void ValveRestoreState::apply(Valve *valve) {
  valve->position = this->position;
  valve->publish_state();
}

}  // namespace valve
}  // namespace esphome
