#include "actuator.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include <strings.h>

namespace esphome::actuator {

static const char *const TAG = "actuator";

// Actuator operation strings indexed by ActuatorOperation enum (0-2): IDLE, OPENING, CLOSING, plus UNKNOWN
PROGMEM_STRING_TABLE(ActuatorOperationStrings, "IDLE", "OPENING", "CLOSING", "UNKNOWN");

//
// ActuatorCallBase
//

ActuatorCallBase &ActuatorCallBase::set_command(const char *command) {
  if (ESPHOME_strcasecmp_P(command, ESPHOME_PSTR("OPEN")) == 0) {
    this->set_command_open();
  } else if (ESPHOME_strcasecmp_P(command, ESPHOME_PSTR("CLOSE")) == 0) {
    this->set_command_close();
  } else if (ESPHOME_strcasecmp_P(command, ESPHOME_PSTR("STOP")) == 0) {
    this->set_command_stop();
  } else if (ESPHOME_strcasecmp_P(command, ESPHOME_PSTR("TOGGLE")) == 0) {
    this->set_command_toggle();
  } else {
    ESP_LOGW(TAG, "'%s' - Unrecognized command %s", this->parent_->get_name().c_str(), command);
  }
  return *this;
}

ActuatorCallBase &ActuatorCallBase::set_command_open() {
  this->position_ = ACTUATOR_OPEN;
  return *this;
}

ActuatorCallBase &ActuatorCallBase::set_command_close() {
  this->position_ = ACTUATOR_CLOSED;
  return *this;
}

ActuatorCallBase &ActuatorCallBase::set_command_stop() {
  this->stop_ = true;
  return *this;
}

ActuatorCallBase &ActuatorCallBase::set_command_toggle() {
  this->toggle_ = true;
  return *this;
}

ActuatorCallBase &ActuatorCallBase::set_position(float position) {
  this->position_ = position;
  return *this;
}

ActuatorCallBase &ActuatorCallBase::set_stop(bool stop) {
  this->stop_ = stop;
  return *this;
}

void ActuatorCallBase::validate_() {
  if (this->position_.has_value()) {
    auto pos = *this->position_;
    if (pos < 0.0f || pos > 1.0f) {
      ESP_LOGW(TAG, "'%s': position %.2f out of range [0.0 - 1.0]", this->parent_->get_name().c_str(), pos);
      this->position_ = clamp(pos, 0.0f, 1.0f);
    }
  }
  if (this->stop_) {
    if (this->position_.has_value() || this->toggle_.has_value()) {
      ESP_LOGW(TAG, "'%s': cannot set position/toggle when stopping", this->parent_->get_name().c_str());
      this->position_.reset();
      this->toggle_.reset();
    }
  }
}

void ActuatorCallBase::call_control_() { this->parent_->control(*this); }

void ActuatorCallBase::perform() {
  ESP_LOGV(TAG, "'%s' - Setting", this->parent_->get_name().c_str());
  this->validate_();
  if (this->stop_) {
    ESP_LOGV(TAG, "  Command: STOP");
  }
  if (this->position_.has_value()) {
    ESP_LOGV(TAG, "  Position: %.0f%%", *this->position_ * 100.0f);
  }
  if (this->toggle_.has_value()) {
    ESP_LOGV(TAG, "  Command: TOGGLE");
  }
  this->call_control_();
}

//
// ActuatorBase
//

bool ActuatorBase::is_fully_open() const { return this->position == ACTUATOR_OPEN; }
bool ActuatorBase::is_fully_closed() const { return this->position == ACTUATOR_CLOSED; }

optional<ActuatorRestoreState> ActuatorBase::restore_state_() {
  this->rtc_ = this->make_entity_preference<ActuatorRestoreState>();
  ActuatorRestoreState recovered{};
  if (!this->rtc_.load(&recovered))
    return {};
  return recovered;
}

}  // namespace esphome::actuator
