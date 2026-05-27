#include "cover.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"
#include <strings.h>

namespace esphome::cover {

static const char *const TAG = "cover";

const LogString *cover_command_to_str(float pos) {
  if (pos == COVER_OPEN) {
    return LOG_STR("OPEN");
  } else if (pos == COVER_CLOSED) {
    return LOG_STR("CLOSE");
  } else {
    return LOG_STR("UNKNOWN");
  }
}
const LogString *cover_operation_to_str(CoverOperation op) { return actuator::actuator_operation_to_str(op); }

Cover::Cover() { this->position = COVER_OPEN; }

//
// CoverCall
//

CoverCall::CoverCall(Cover *parent) : actuator::ActuatorCallBase(parent) {}

// Covariant wrappers — call parent method, return CoverCall& for fluent chaining
CoverCall &CoverCall::set_command(const char *command) {
  actuator::ActuatorCallBase::set_command(command);
  return *this;
}
CoverCall &CoverCall::set_command_open() {
  actuator::ActuatorCallBase::set_command_open();
  return *this;
}
CoverCall &CoverCall::set_command_close() {
  actuator::ActuatorCallBase::set_command_close();
  return *this;
}
CoverCall &CoverCall::set_command_stop() {
  actuator::ActuatorCallBase::set_command_stop();
  return *this;
}
CoverCall &CoverCall::set_command_toggle() {
  actuator::ActuatorCallBase::set_command_toggle();
  return *this;
}
CoverCall &CoverCall::set_position(float position) {
  actuator::ActuatorCallBase::set_position(position);
  return *this;
}
CoverCall &CoverCall::set_tilt(float tilt) {
  this->tilt_ = tilt;
  return *this;
}
CoverCall &CoverCall::set_stop(bool stop) {
  actuator::ActuatorCallBase::set_stop(stop);
  return *this;
}

void CoverCall::perform() {
  ESP_LOGV(TAG, "'%s' - Setting", this->parent_->get_name().c_str());
  auto traits = static_cast<Cover *>(this->parent_)->get_traits();
  this->CoverCall::validate();
  if (this->stop_) {
    ESP_LOGV(TAG, "  Command: STOP");
  }
  if (this->position_.has_value()) {
    if (traits.get_supports_position()) {
      ESP_LOGV(TAG, "  Position: %.0f%%", *this->position_ * 100.0f);
    } else {
      ESP_LOGV(TAG, "  Command: %s", LOG_STR_ARG(cover_command_to_str(*this->position_)));
    }
  }
  if (this->tilt_.has_value()) {
    ESP_LOGV(TAG, "  Tilt: %.0f%%", *this->tilt_ * 100.0f);
  }
  if (this->toggle_.has_value()) {
    ESP_LOGV(TAG, "  Command: TOGGLE");
  }
  static_cast<Cover *>(this->parent_)->control(*this);
}

void CoverCall::validate() {
  auto traits = static_cast<Cover *>(this->parent_)->get_traits();
  const char *name = this->parent_->get_name().c_str();

  if (this->position_.has_value()) {
    auto pos = *this->position_;
    if (!traits.get_supports_position() && pos != COVER_OPEN && pos != COVER_CLOSED) {
      ESP_LOGW(TAG, "'%s': position unsupported", name);
      this->position_.reset();
    } else if (pos < 0.0f || pos > 1.0f) {
      ESP_LOGW(TAG, "'%s': position %.2f out of range", name, pos);
      this->position_ = clamp(pos, 0.0f, 1.0f);
    }
  }
  if (this->tilt_.has_value()) {
    auto tilt = *this->tilt_;
    if (!traits.get_supports_tilt()) {
      ESP_LOGW(TAG, "'%s': tilt unsupported", name);
      this->tilt_.reset();
    } else if (tilt < 0.0f || tilt > 1.0f) {
      ESP_LOGW(TAG, "'%s': tilt %.2f out of range", name, tilt);
      this->tilt_ = clamp(tilt, 0.0f, 1.0f);
    }
  }
  if (this->toggle_.has_value()) {
    if (!traits.get_supports_toggle()) {
      ESP_LOGW(TAG, "'%s': toggle unsupported", name);
      this->toggle_.reset();
    }
  }
  if (this->stop_) {
    if (this->position_.has_value() || this->tilt_.has_value() || this->toggle_.has_value()) {
      ESP_LOGW(TAG, "'%s': cannot position/tilt/toggle when stopping", name);
      this->position_.reset();
      this->tilt_.reset();
      this->toggle_.reset();
    }
  }
}

CoverCall Cover::make_call() { return CoverCall(this); }

void Cover::publish_state(bool save) {
  this->position = clamp(this->position, 0.0f, 1.0f);
  this->tilt = clamp(this->tilt, 0.0f, 1.0f);

  ESP_LOGV(TAG, "'%s' >>", this->name_.c_str());
  auto traits = this->get_traits();
  if (traits.get_supports_position()) {
    ESP_LOGV(TAG, "  Position: %.0f%%", this->position * 100.0f);
  } else {
    if (this->position == COVER_OPEN) {
      ESP_LOGV(TAG, "  State: OPEN");
    } else if (this->position == COVER_CLOSED) {
      ESP_LOGV(TAG, "  State: CLOSED");
    } else {
      ESP_LOGV(TAG, "  State: UNKNOWN");
    }
  }
  if (traits.get_supports_tilt()) {
    ESP_LOGV(TAG, "  Tilt: %.0f%%", this->tilt * 100.0f);
  }
  ESP_LOGV(TAG, "  Current Operation: %s", LOG_STR_ARG(cover_operation_to_str(this->current_operation)));

  this->state_callback_.call();
#if defined(USE_COVER) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_cover_update(this);
#endif

  if (save) {
    CoverRestoreState restore{};
    memset(&restore, 0, sizeof(restore));
    restore.position = this->position;
    if (traits.get_supports_tilt()) {
      restore.tilt = this->tilt;
    }
    this->rtc_.save(&restore);
  }
}

optional<float> Cover::do_restore_state() {
  auto restore = this->restore_state_();
  if (!restore.has_value())
    return {};
  restore->apply(this);
  float pos = restore->position;  // copy to avoid packed-field reference
  return pos;
}

CoverCall CoverRestoreState::to_call(Cover *cover) {
  auto call = cover->make_call();
  auto traits = cover->get_traits();
  call.set_position(this->position);
  if (traits.get_supports_tilt())
    call.set_tilt(this->tilt);
  return call;
}
void CoverRestoreState::apply(Cover *cover) {
  cover->position = this->position;
  cover->tilt = this->tilt;
  cover->publish_state();
}

}  // namespace esphome::cover
