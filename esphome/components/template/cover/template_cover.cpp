#include "template_cover.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

using namespace esphome::cover;

static const char *const TAG = "template.cover";

TemplateCover::TemplateCover()
    : open_trigger_(new Trigger<>()),
      close_trigger_(new Trigger<>),
      stop_trigger_(new Trigger<>()),
      position_trigger_(new Trigger<float>()),
      tilt_trigger_(new Trigger<float>()) {}
void TemplateCover::setup() {
  ESP_LOGCONFIG(TAG, "Setting up template cover '%s'...", this->name_.c_str());
  switch (this->restore_mode_) {
    case COVER_NO_RESTORE:
      break;
    case COVER_RESTORE: {
      auto restore = this->restore_state_();
      if (restore.has_value())
        restore->apply(this);
      break;
    }
    case COVER_RESTORE_AND_CALL: {
      auto restore = this->restore_state_();
      if (restore.has_value()) {
        restore->to_call(this).perform();
      }
      break;
    }
  }
}
void TemplateCover::loop() {
  bool changed = false;

  if (this->state_f_.has_value()) {
    auto s = (*this->state_f_)();
    if (s.has_value()) {
      auto pos = clamp(*s, 0.0f, 1.0f);
      if (pos != this->position) {
        this->position = pos;
        changed = true;
      }
    }
  }
  if (this->tilt_f_.has_value()) {
    auto s = (*this->tilt_f_)();
    if (s.has_value()) {
      auto tilt = clamp(*s, 0.0f, 1.0f);
      if (tilt != this->tilt) {
        this->tilt = tilt;
        changed = true;
      }
    }
  }

  if (changed)
    this->publish_state();
}
void TemplateCover::set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
void TemplateCover::set_assumed_state(bool assumed_state) { this->assumed_state_ = assumed_state; }
void TemplateCover::set_state_lambda(std::function<optional<float>()> &&f) { this->state_f_ = f; }
float TemplateCover::get_setup_priority() const { return setup_priority::HARDWARE; }
Trigger<> *TemplateCover::get_open_trigger() const { return this->open_trigger_; }
Trigger<> *TemplateCover::get_close_trigger() const { return this->close_trigger_; }
Trigger<> *TemplateCover::get_stop_trigger() const { return this->stop_trigger_; }
void TemplateCover::dump_config() { LOG_COVER("", "Template Cover", this); }
void TemplateCover::control(const CoverCall &call) {
  if (call.get_stop()) {
    this->stop_prev_trigger_();
    this->stop_trigger_->trigger();
    this->prev_command_trigger_ = this->stop_trigger_;
    this->publish_state();
  }
  if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    this->stop_prev_trigger_();

    if (pos == COVER_OPEN) {
      this->open_trigger_->trigger();
      this->prev_command_trigger_ = this->open_trigger_;
    } else if (pos == COVER_CLOSED) {
      this->close_trigger_->trigger();
      this->prev_command_trigger_ = this->close_trigger_;
    } else {
      this->position_trigger_->trigger(pos);
    }

    if (this->optimistic_) {
      this->position = pos;
    }
  }

  if (call.get_tilt().has_value()) {
    auto tilt = *call.get_tilt();
    this->tilt_trigger_->trigger(tilt);

    if (this->optimistic_) {
      this->tilt = tilt;
    }
  }

  this->publish_state();
}
CoverTraits TemplateCover::get_traits() {
  auto traits = CoverTraits();
  traits.set_is_assumed_state(this->assumed_state_);
  traits.set_supports_stop(this->has_stop_);
  traits.set_supports_position(this->has_position_);
  traits.set_supports_tilt(this->has_tilt_);
  return traits;
}
Trigger<float> *TemplateCover::get_position_trigger() const { return this->position_trigger_; }
Trigger<float> *TemplateCover::get_tilt_trigger() const { return this->tilt_trigger_; }
void TemplateCover::set_tilt_lambda(std::function<optional<float>()> &&tilt_f) { this->tilt_f_ = tilt_f; }
void TemplateCover::set_has_stop(bool has_stop) { this->has_stop_ = has_stop; }
void TemplateCover::set_has_position(bool has_position) { this->has_position_ = has_position; }
void TemplateCover::set_has_tilt(bool has_tilt) { this->has_tilt_ = has_tilt; }
void TemplateCover::stop_prev_trigger_() {
  if (this->prev_command_trigger_ != nullptr) {
    this->prev_command_trigger_->stop_action();
    this->prev_command_trigger_ = nullptr;
  }
}

}  // namespace template_
}  // namespace esphome
