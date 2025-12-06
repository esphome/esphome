#include "cover.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"

#include <strings.h>

#include "esphome/core/log.h"

namespace esphome::cover {

static const char *const TAG = "cover";

const float COVER_OPEN = 1.0f;
const float COVER_CLOSED = 0.0f;

const LogString *cover_command_to_str(float pos) {
  if (pos == COVER_OPEN) {
    return LOG_STR("OPEN");
  } else if (pos == COVER_CLOSED) {
    return LOG_STR("CLOSE");
  } else {
    return LOG_STR("UNKNOWN");
  }
}
const LogString *cover_operation_to_str(CoverOperation op) {
  switch (op) {
    case COVER_OPERATION_IDLE:
      return LOG_STR("IDLE");
    case COVER_OPERATION_OPENING:
      return LOG_STR("OPENING");
    case COVER_OPERATION_CLOSING:
      return LOG_STR("CLOSING");
    default:
      return LOG_STR("UNKNOWN");
  }
}

Cover::Cover() : position{COVER_OPEN} {}

CoverCall::CoverCall(Cover *parent) : parent_(parent) {}
CoverCall &CoverCall::set_command(const char *command) {
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
CoverCall &CoverCall::set_command_open() {
  this->position_ = COVER_OPEN;
  return *this;
}
CoverCall &CoverCall::set_command_close() {
  this->position_ = COVER_CLOSED;
  return *this;
}
CoverCall &CoverCall::set_command_stop() {
  this->stop_ = true;
  return *this;
}
CoverCall &CoverCall::set_command_toggle() {
  this->toggle_ = true;
  return *this;
}
CoverCall &CoverCall::set_position(float position) {
  this->position_ = position;
  return *this;
}
CoverCall &CoverCall::set_tilt(float tilt) {
  this->tilt_ = tilt;
  return *this;
}
void CoverCall::perform() {
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
      ESP_LOGD(TAG, "  Command: %s", LOG_STR_ARG(cover_command_to_str(*this->position_)));
    }
  }
  if (this->tilt_.has_value()) {
    ESP_LOGD(TAG, "  Tilt: %.0f%%", *this->tilt_ * 100.0f);
  }
  if (this->toggle_.has_value()) {
    ESP_LOGD(TAG, "  Command: TOGGLE");
  }
  this->parent_->control(*this);
}
const optional<float> &CoverCall::get_position() const { return this->position_; }
const optional<float> &CoverCall::get_tilt() const { return this->tilt_; }
const optional<bool> &CoverCall::get_toggle() const { return this->toggle_; }
void CoverCall::validate_() {
  auto traits = this->parent_->get_traits();
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
CoverCall &CoverCall::set_stop(bool stop) {
  this->stop_ = stop;
  return *this;
}
bool CoverCall::get_stop() const { return this->stop_; }

CoverCall Cover::make_call() { return {this}; }

void Cover::add_on_state_callback(std::function<void()> &&f) { this->state_callback_.add(std::move(f)); }
void Cover::publish_state(bool save) {
  this->position = clamp(this->position, 0.0f, 1.0f);
  this->tilt = clamp(this->tilt, 0.0f, 1.0f);

  ESP_LOGD(TAG, "'%s' - Publishing:", this->name_.c_str());
  auto traits = this->get_traits();
  if (traits.get_supports_position()) {
    ESP_LOGD(TAG, "  Position: %.0f%%", this->position * 100.0f);
  } else {
    if (this->position == COVER_OPEN) {
      ESP_LOGD(TAG, "  State: OPEN");
    } else if (this->position == COVER_CLOSED) {
      ESP_LOGD(TAG, "  State: CLOSED");
    } else {
      ESP_LOGD(TAG, "  State: UNKNOWN");
    }
  }
  if (traits.get_supports_tilt()) {
    ESP_LOGD(TAG, "  Tilt: %.0f%%", this->tilt * 100.0f);
  }
  ESP_LOGD(TAG, "  Current Operation: %s", LOG_STR_ARG(cover_operation_to_str(this->current_operation)));

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
optional<CoverRestoreState> Cover::restore_state_() {
  this->rtc_ = global_preferences->make_preference<CoverRestoreState>(this->get_preference_hash());
  CoverRestoreState recovered{};
  if (!this->rtc_.load(&recovered))
    return {};
  return recovered;
}

bool Cover::is_fully_open() const { return this->position == COVER_OPEN; }
bool Cover::is_fully_closed() const { return this->position == COVER_CLOSED; }

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
