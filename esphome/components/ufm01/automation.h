#pragma once

#include "esphome/core/defines.h"

#ifdef USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION

#include "ufm01.h"

#include "esphome/core/automation.h"

#include <tuple>

namespace esphome::ufm01 {

template<typename... Ts>
class ClearAccumulatedFlowAction final : public Action<Ts...>, public ClearAccumulatedFlowActionInterface {
 public:
  explicit ClearAccumulatedFlowAction(UFM01Component *parent) : parent_(parent) {}

  void play_complex(const Ts &...x) override {
    if (this->waiting_)
      return;
    this->num_running_++;
    this->waiting_ = true;
    this->args_ = std::make_tuple(x...);
    if (!this->parent_->request_clear_accumulated_flow_(this)) {
      this->waiting_ = false;
      this->num_running_--;
    }
  }

  void complete() override {
    if (!this->waiting_)
      return;
    this->waiting_ = false;
    this->play_next_tuple_(this->args_);
  }

 protected:
  void play(const Ts &...) override { /* unused; see play_complex */
  }

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

#endif  // USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION
