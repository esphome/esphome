#include "valve.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"
#include <strings.h>

namespace esphome::valve {

static const char *const TAG = "valve";

const LogString *valve_command_to_str(float pos) {
  if (pos == VALVE_OPEN) {
    return LOG_STR("OPEN");
  } else if (pos == VALVE_CLOSED) {
    return LOG_STR("CLOSE");
  } else {
    return LOG_STR("UNKNOWN");
  }
}
const LogString *valve_operation_to_str(ValveOperation op) { return actuator::actuator_operation_to_str(op); }

Valve::Valve() { this->position = VALVE_OPEN; }

//
// ValveCall
//

ValveCall::ValveCall(Valve *parent) : actuator::ActuatorCallBase(parent) {}

// Covariant wrappers — call parent method, return ValveCall& for fluent chaining
ValveCall &ValveCall::set_command(const char *command) {
  actuator::ActuatorCallBase::set_command(command);
  return *this;
}
ValveCall &ValveCall::set_command_open() {
  actuator::ActuatorCallBase::set_command_open();
  return *this;
}
ValveCall &ValveCall::set_command_close() {
  actuator::ActuatorCallBase::set_command_close();
  return *this;
}
ValveCall &ValveCall::set_command_stop() {
  actuator::ActuatorCallBase::set_command_stop();
  return *this;
}
ValveCall &ValveCall::set_command_toggle() {
  actuator::ActuatorCallBase::set_command_toggle();
  return *this;
}
ValveCall &ValveCall::set_position(float position) {
  actuator::ActuatorCallBase::set_position(position);
  return *this;
}
ValveCall &ValveCall::set_stop(bool stop) {
  actuator::ActuatorCallBase::set_stop(stop);
  return *this;
}

void ValveCall::perform() {
  ESP_LOGV(TAG, "'%s' - Setting", this->parent_->get_name().c_str());
  auto traits = static_cast<Valve *>(this->parent_)->get_traits();
  this->ValveCall::validate();
  if (this->stop_) {
    ESP_LOGV(TAG, "  Command: STOP");
  }
  if (this->position_.has_value()) {
    if (traits.get_supports_position()) {
      ESP_LOGV(TAG, "  Position: %.0f%%", *this->position_ * 100.0f);
    } else {
      ESP_LOGV(TAG, "  Command: %s", LOG_STR_ARG(valve_command_to_str(*this->position_)));
    }
  }
  if (this->toggle_.has_value()) {
    ESP_LOGV(TAG, "  Command: TOGGLE");
  }
  static_cast<Valve *>(this->parent_)->control(*this);
}

void ValveCall::validate() {
  auto traits = static_cast<Valve *>(this->parent_)->get_traits();

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

ValveCall Valve::make_call() { return ValveCall(this); }

void Valve::publish_state(bool save) {
  this->position = clamp(this->position, 0.0f, 1.0f);

  ESP_LOGV(TAG, "'%s' >>", this->name_.c_str());
  auto traits = this->get_traits();
  if (traits.get_supports_position()) {
    ESP_LOGV(TAG, "  Position: %.0f%%", this->position * 100.0f);
  } else {
    if (this->position == VALVE_OPEN) {
      ESP_LOGV(TAG, "  State: OPEN");
    } else if (this->position == VALVE_CLOSED) {
      ESP_LOGV(TAG, "  State: CLOSED");
    } else {
      ESP_LOGV(TAG, "  State: UNKNOWN");
    }
  }
  ESP_LOGV(TAG, "  Current Operation: %s", LOG_STR_ARG(valve_operation_to_str(this->current_operation)));

  this->state_callback_.call();
#if defined(USE_VALVE) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_valve_update(this);
#endif

  if (save) {
    ValveRestoreState restore{};
    memset(&restore, 0, sizeof(restore));
    restore.position = this->position;
    this->rtc_.save(&restore);
  }
}

bool Valve::is_fully_open() const { return this->position == VALVE_OPEN; }
bool Valve::is_fully_closed() const { return this->position == VALVE_CLOSED; }

optional<float> Valve::do_restore_state() {
  auto restore = this->restore_state_();
  if (!restore.has_value())
    return {};
  restore->apply(this);
  float pos = restore->position;  // copy to avoid packed-field reference
  return pos;
}

ValveCall ValveRestoreState::to_call(Valve *valve) {
  auto call = valve->make_call();
  call.set_position(this->position);
  return call;
}
void ValveRestoreState::apply(Valve *valve) {
  valve->position = this->position;
  valve->publish_state();
}

}  // namespace esphome::valve
