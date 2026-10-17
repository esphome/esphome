#pragma once

#include <cinttypes>

#include "esphome/core/automation.h"
#include "esphome/core/log.h"

#include "m5pm1.h"

namespace esphome::m5pm1 {

template<typename... Ts> class M5PM1TimerStartAction final : public Action<Ts...> {
 public:
  explicit M5PM1TimerStartAction(M5PM1Component *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(uint32_t, duration)
  TEMPLATABLE_VALUE(M5PM1TimerAction, timer_action)

  Trigger<> *get_success_trigger() { return &this->success_trigger_; }
  Trigger<> *get_error_trigger() { return &this->error_trigger_; }

 protected:
  void play(const Ts &...x) override {
    static const char *const tag = "m5pm1.timer";
    const uint32_t duration = this->duration_.value(x...);
    const M5PM1TimerAction timer_action = this->timer_action_.value(x...);
    const uint8_t timer_action_value = static_cast<uint8_t>(timer_action);

    if (duration == 0 || duration > M5PM1_TIMER_MAX_SECONDS ||
        timer_action_value < static_cast<uint8_t>(M5PM1TimerAction::M5PM1_TIMER_ACTION_SET_WAKEUP_FLAG) ||
        timer_action_value > static_cast<uint8_t>(M5PM1TimerAction::M5PM1_TIMER_ACTION_POWER_OFF)) {
      this->error_trigger_.trigger();
      return;
    }

    if (!this->parent_->set_timer(duration, timer_action)) {
      this->error_trigger_.trigger();
      return;
    }

    this->success_trigger_.trigger();
  }

  M5PM1Component *parent_;
  Trigger<> success_trigger_;
  Trigger<> error_trigger_;
};

template<typename... Ts> class M5PM1TimerClearAction final : public Action<Ts...> {
 public:
  explicit M5PM1TimerClearAction(M5PM1Component *parent) : parent_(parent) {}

  Trigger<> *get_success_trigger() { return &this->success_trigger_; }
  Trigger<> *get_error_trigger() { return &this->error_trigger_; }

 protected:
  void play(const Ts &...) override {
    static const char *const tag = "m5pm1.timer";
    if (!this->parent_->clear_timer()) {
      this->error_trigger_.trigger();
      return;
    }
    this->success_trigger_.trigger();
  }

  M5PM1Component *parent_;
  Trigger<> success_trigger_;
  Trigger<> error_trigger_;
};

template<typename... Ts> class M5PM1SystemCommandAction final : public Action<Ts...> {
 public:
  M5PM1SystemCommandAction(M5PM1Component *parent, M5PM1SystemCommand command) : parent_(parent), command_(command) {}

  Trigger<> *get_success_trigger() { return &this->success_trigger_; }
  Trigger<> *get_error_trigger() { return &this->error_trigger_; }

 protected:
  void play(const Ts &...) override {
    static const char *const tag = "m5pm1.command";
    const uint8_t command = static_cast<uint8_t>(this->command_);
    if (!this->parent_->system_command(this->command_)) {
      this->error_trigger_.trigger();
      return;
    }
    this->success_trigger_.trigger();
  }

  M5PM1Component *parent_;
  M5PM1SystemCommand command_;
  Trigger<> success_trigger_;
  Trigger<> error_trigger_;
};

}  // namespace esphome::m5pm1
