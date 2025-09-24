#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "cover.h"

namespace esphome {
namespace cover {

template<typename... Ts> class OpenAction : public Action<Ts...> {
 public:
  explicit OpenAction(Cover *cover) : cover_(cover) {}

  void play(Ts... x) override { this->cover_->make_call().set_command_open().perform(); }

 protected:
  Cover *cover_;
};

template<typename... Ts> class CloseAction : public Action<Ts...> {
 public:
  explicit CloseAction(Cover *cover) : cover_(cover) {}

  void play(Ts... x) override { this->cover_->make_call().set_command_close().perform(); }

 protected:
  Cover *cover_;
};

template<typename... Ts> class StopAction : public Action<Ts...> {
 public:
  explicit StopAction(Cover *cover) : cover_(cover) {}

  void play(Ts... x) override { this->cover_->make_call().set_command_stop().perform(); }

 protected:
  Cover *cover_;
};

template<typename... Ts> class ToggleAction : public Action<Ts...> {
 public:
  explicit ToggleAction(Cover *cover) : cover_(cover) {}

  void play(Ts... x) override { this->cover_->make_call().set_command_toggle().perform(); }

 protected:
  Cover *cover_;
};

template<typename... Ts> class ControlAction : public Action<Ts...> {
 public:
  explicit ControlAction(Cover *cover) : cover_(cover) {}

  TEMPLATABLE_VALUE(bool, stop)
  TEMPLATABLE_VALUE(float, position)
  TEMPLATABLE_VALUE(float, tilt)

  void play(Ts... x) override {
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

  void play(Ts... x) override {
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

template<typename... Ts> class CoverIsOpenCondition : public Condition<Ts...> {
 public:
  CoverIsOpenCondition(Cover *cover) : cover_(cover) {}
  bool check(Ts... x) override { return this->cover_->is_fully_open(); }

 protected:
  Cover *cover_;
};

template<typename... Ts> class CoverIsClosedCondition : public Condition<Ts...> {
 public:
  CoverIsClosedCondition(Cover *cover) : cover_(cover) {}
  bool check(Ts... x) override { return this->cover_->is_fully_closed(); }

 protected:
  Cover *cover_;
};

class CoverOpenTrigger : public Trigger<> {
 public:
  CoverOpenTrigger(Cover *a_cover) {
    a_cover->add_on_state_callback([this, a_cover]() {
      if (a_cover->is_fully_open()) {
        this->trigger();
      }
    });
  }
};

class CoverClosedTrigger : public Trigger<> {
 public:
  CoverClosedTrigger(Cover *a_cover) {
    a_cover->add_on_state_callback([this, a_cover]() {
      if (a_cover->is_fully_closed()) {
        this->trigger();
      }
    });
  }
};

}  // namespace cover
}  // namespace esphome
