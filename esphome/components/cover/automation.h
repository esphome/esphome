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

template<typename... Ts> class ControlAction : public Action<Ts...> {
 public:
  explicit ControlAction(Cover *cover) : cover_(cover) {}

  TEMPLATABLE_VALUE(bool, stop)
  TEMPLATABLE_VALUE(float, position)
  TEMPLATABLE_VALUE(float, tilt)

  void play(const Ts &...x) override {
    auto call = this->cover_->make_call();
    if (this->stop_.has_value())
      call.set_stop(this->stop_.value(x...));
    if (this->position_.has_value())
      call.set_position(this->position_.value(x...));
    if (this->tilt_.has_value())
      call.set_tilt(this->tilt_.value(x...));
    call.perform();
  }

 protected:
  Cover *cover_;
};

template<typename... Ts> class CoverPublishAction : public Action<Ts...> {
 public:
  CoverPublishAction(Cover *cover) : cover_(cover) {}
  TEMPLATABLE_VALUE(float, position)
  TEMPLATABLE_VALUE(float, tilt)
  TEMPLATABLE_VALUE(CoverOperation, current_operation)

  void play(const Ts &...x) override {
    if (this->position_.has_value())
      this->cover_->position = this->position_.value(x...);
    if (this->tilt_.has_value())
      this->cover_->tilt = this->tilt_.value(x...);
    if (this->current_operation_.has_value())
      this->cover_->current_operation = this->current_operation_.value(x...);
    this->cover_->publish_state();
  }

 protected:
  Cover *cover_;
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
  CoverPositionTrigger(Cover *a_cover) {
    a_cover->add_on_state_callback([this, a_cover]() {
      if (a_cover->position != this->last_position_) {
        this->last_position_ = a_cover->position;
        if (a_cover->position == (OPEN ? COVER_OPEN : COVER_CLOSED))
          this->trigger();
      }
    });
  }

 protected:
  float last_position_{NAN};
};

using CoverOpenedTrigger = CoverPositionTrigger<true>;
using CoverClosedTrigger = CoverPositionTrigger<false>;

template<CoverOperation OP> class CoverTrigger : public Trigger<> {
 public:
  CoverTrigger(Cover *a_cover) {
    a_cover->add_on_state_callback([this, a_cover]() {
      auto current_op = a_cover->current_operation;
      if (current_op == OP) {
        if (!this->last_operation_.has_value() || this->last_operation_.value() != OP) {
          this->trigger();
        }
      }
      this->last_operation_ = current_op;
    });
  }

 protected:
  optional<CoverOperation> last_operation_{};
};
}  // namespace esphome::cover
