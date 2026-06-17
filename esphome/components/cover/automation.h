#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "cover.h"

namespace esphome::cover {

template<typename... Ts> class OpenAction : public Action<Ts...> {
 public:
  explicit OpenAction(Cover *cover) : cover_(cover) {}

  void play(const Ts &...x) override { this->cover_->make_call().set_command_open().perform(); }

 protected:
  Cover *cover_;
};

template<typename... Ts> class CloseAction : public Action<Ts...> {
 public:
  explicit CloseAction(Cover *cover) : cover_(cover) {}

  void play(const Ts &...x) override { this->cover_->make_call().set_command_close().perform(); }

 protected:
  Cover *cover_;
};

template<typename... Ts> class StopAction : public Action<Ts...> {
 public:
  explicit StopAction(Cover *cover) : cover_(cover) {}

  void play(const Ts &...x) override { this->cover_->make_call().set_command_stop().perform(); }

 protected:
  Cover *cover_;
};

template<typename... Ts> class ToggleAction : public Action<Ts...> {
 public:
  explicit ToggleAction(Cover *cover) : cover_(cover) {}

  void play(const Ts &...x) override { this->cover_->make_call().set_command_toggle().perform(); }

 protected:
  Cover *cover_;
};

// All configured fields are baked into a single stateless lambda whose
// constants live in flash. Each action stores only one function pointer
// plus one parent pointer, regardless of how many fields the user set.
// Trigger args are forwarded to the apply function so user lambdas
// (e.g. `position: !lambda "return x;"`) keep working.
//
// Trigger args are normalized to `const std::remove_cvref_t<Ts> &...` so
// the codegen can emit a matching parameter list for both the apply lambda
// and any inner field lambdas without producing invalid C++ source text
// (e.g. `const T & &` if Ts already carries a reference, or `const const
// T &` if Ts already carries a const). This keeps trigger args no-copy
// regardless of whether the trigger supplies `T`, `T &`, or `const T &`.

template<typename... Ts> class ControlAction : public Action<Ts...> {
 public:
  using ApplyFn = void (*)(CoverCall &, const std::remove_cvref_t<Ts> &...);
  ControlAction(Cover *cover, ApplyFn apply) : cover_(cover), apply_(apply) {}

  void play(const Ts &...x) override {
    auto call = this->cover_->make_call();
    this->apply_(call, x...);
    call.perform();
  }

 protected:
  Cover *cover_;
  ApplyFn apply_;
};

template<typename... Ts> class CoverPublishAction : public Action<Ts...> {
 public:
  using ApplyFn = void (*)(Cover *, const std::remove_cvref_t<Ts> &...);
  CoverPublishAction(Cover *cover, ApplyFn apply) : cover_(cover), apply_(apply) {}

  void play(const Ts &...x) override {
    this->apply_(this->cover_, x...);
    this->cover_->publish_state();
  }

 protected:
  Cover *cover_;
  ApplyFn apply_;
};

template<bool OPEN, typename... Ts> class CoverPositionCondition : public Condition<Ts...> {
 public:
  CoverPositionCondition(Cover *cover) : cover_(cover) {}

  bool check(const Ts &...x) override { return this->cover_->position == (OPEN ? COVER_OPEN : COVER_CLOSED); }

 protected:
  Cover *cover_;
};

template<typename... Ts> using CoverIsOpenCondition = CoverPositionCondition<true, Ts...>;
template<typename... Ts> using CoverIsClosedCondition = CoverPositionCondition<false, Ts...>;

template<bool OPEN> class CoverPositionTrigger : public Trigger<> {
 public:
  CoverPositionTrigger(Cover *a_cover) : cover_(a_cover) {
    a_cover->add_on_state_callback([this]() {
      if (this->cover_->position != this->last_position_) {
        this->last_position_ = this->cover_->position;
        if (this->cover_->position == (OPEN ? COVER_OPEN : COVER_CLOSED))
          this->trigger();
      }
    });
  }

 protected:
  Cover *cover_;
  float last_position_{NAN};
};

using CoverOpenedTrigger = CoverPositionTrigger<true>;
using CoverClosedTrigger = CoverPositionTrigger<false>;

template<CoverOperation OP> class CoverTrigger : public Trigger<> {
 public:
  CoverTrigger(Cover *a_cover) : cover_(a_cover) {
    a_cover->add_on_state_callback([this]() {
      auto current_op = this->cover_->current_operation;
      if (current_op == OP) {
        if (!this->last_operation_.has_value() || this->last_operation_.value() != OP) {
          this->trigger();
        }
      }
      this->last_operation_ = current_op;
    });
  }

 protected:
  Cover *cover_;
  optional<CoverOperation> last_operation_{};
};
}  // namespace esphome::cover
