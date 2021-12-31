#include "template_lock.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

using namespace esphome::lock;

static const char *const TAG = "template.lock";

TemplateLock::TemplateLock()
    : lock_trigger_(new Trigger<>()), unlock_trigger_(new Trigger<>()), open_trigger_(new Trigger<>()) {}

void TemplateLock::loop() {
  if (!this->f_.has_value())
    return;
  auto val = (*this->f_)();
  if (!val.has_value())
    return;

  this->publish_state(*val);
}
void TemplateLock::write_state(LockState state) {
  if (this->prev_trigger_ != nullptr) {
    this->prev_trigger_->stop_action();
  }

  if (state == LOCK_STATE_LOCKED) {
    this->prev_trigger_ = this->lock_trigger_;
    this->lock_trigger_->trigger();
  } else if (state == LOCK_STATE_UNLOCKED) {
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
    this->publish_state(LOCK_STATE_UNLOCKED);
}
void TemplateLock::set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
bool TemplateLock::assumed_state() { return this->assumed_state_; }
void TemplateLock::set_state_lambda(std::function<optional<LockState>()> &&f) { this->f_ = f; }
float TemplateLock::get_setup_priority() const { return setup_priority::HARDWARE; }
Trigger<> *TemplateLock::get_lock_trigger() const { return this->lock_trigger_; }
Trigger<> *TemplateLock::get_unlock_trigger() const { return this->unlock_trigger_; }
Trigger<> *TemplateLock::get_open_trigger() const { return this->open_trigger_; }
void TemplateLock::dump_config() {
  LOG_LOCK("", "Template Lock", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
}
void TemplateLock::set_assumed_state(bool assumed_state) { this->assumed_state_ = assumed_state; }
void TemplateLock::set_supports_open(bool supports_open) { this->supports_open = supports_open; }

}  // namespace template_
}  // namespace esphome
