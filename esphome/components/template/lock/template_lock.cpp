#include "template_lock.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.lock";

TemplateLock::TemplateLock()
    : lock_trigger_(new Trigger<>()), unlock_trigger_(new Trigger<>()), open_trigger_(new Trigger<>()) {}

void TemplateLock::loop() {
  if (!this->f_.has_value())
    return;
  auto s = (*this->f_)();
  if (!s.has_value())
    return;

  this->publish_state(*s);
}
void TemplateLock::write_state(bool state) {
  if (this->prev_trigger_ != nullptr) {
    this->prev_trigger_->stop_action();
  }

  if (state) {
    this->prev_trigger_ = this->lock_trigger_;
    this->lock_trigger_->trigger();
  } else {
    this->prev_trigger_ = this->unlock_trigger_;
    this->unlock_trigger_->trigger();
  }

  if (this->optimistic_)
    this->publish_state(state);
}
void TemplateLock::open_latch() {
  if (this->prev_trigger_ != nullptr) {
    this->prev_trigger_->stop_action();
  }
  this->prev_trigger_ = this->open_trigger_;
  this->open_trigger_->trigger();

  if (this->optimistic_)
    this->publish_state(false);
}
void TemplateLock::set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
bool TemplateLock::assumed_state() { return this->assumed_state_; }
void TemplateLock::set_state_lambda(std::function<optional<bool>()> &&f) { this->f_ = f; }
float TemplateLock::get_setup_priority() const { return setup_priority::HARDWARE; }
Trigger<> *TemplateLock::get_lock_trigger() const { return this->lock_trigger_; }
Trigger<> *TemplateLock::get_unlock_trigger() const { return this->unlock_trigger_; }
Trigger<> *TemplateLock::get_open_trigger() const { return this->open_trigger_; }
void TemplateLock::setup() {
  if (!this->restore_state_)
    return;

  auto restored = this->get_initial_state();
  if (!restored.has_value())
    return;

  ESP_LOGD(TAG, "  Restored state %s", LOCKUNLOCK(*restored));
  if (*restored) {
    this->lock();
  } else {
    this->unlock();
  }
}
void TemplateLock::dump_config() {
  LOG_LOCK("", "Template Lock", this);
  ESP_LOGCONFIG(TAG, "  Restore State: %s", YESNO(this->restore_state_));
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
}
void TemplateLock::set_restore_state(bool restore_state) { this->restore_state_ = restore_state; }
void TemplateLock::set_assumed_state(bool assumed_state) { this->assumed_state_ = assumed_state; }

}  // namespace template_
}  // namespace esphome
