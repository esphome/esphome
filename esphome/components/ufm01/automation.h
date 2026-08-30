#pragma once

#include "ufm01.h"

#include "esphome/core/automation.h"

#include <tuple>

namespace esphome::ufm01 {

template<typename... Ts>
class ClearAccumulatedFlowAction final : public Action<Ts...>, public ClearAccumulatedFlowActionInterface {
 public:
  explicit ClearAccumulatedFlowAction(UFM01Component *parent) : parent_(parent) {}

  void play_complex(const Ts &...x) override {
    this->num_running_++;
    if (this->waiting_) {
      this->play_next_(x...);
      return;
    }
    this->waiting_ = true;
    this->args_ = std::make_tuple(x...);
    this->parent_->request_clear_accumulated_flow_(this);
  }

  void complete() override {
    if (!this->waiting_)
      return;
    this->waiting_ = false;
    std::apply([this](const Ts &...x) { this->play_next_(x...); }, this->args_);
  }

 protected:
  void play(const Ts &...) override { /* unused; see play_complex */ }

  void stop() override {
    if (!this->waiting_)
      return;
    this->waiting_ = false;
    this->parent_->cancel_pending_clear_action_(this);
  }

  UFM01Component *parent_;
  std::tuple<Ts...> args_{};
  bool waiting_{false};
};

}  // namespace esphome::ufm01
