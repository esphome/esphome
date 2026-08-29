#pragma once

#include "ufm01.h"

#include "esphome/core/automation.h"
#include "esphome/core/base_automation.h"

#include <tuple>

namespace esphome::ufm01 {

class ClearAccumulatedFlowActionInterface {
 public:
  virtual ~ClearAccumulatedFlowActionInterface() = default;
  virtual void complete() = 0;
};

template<typename... Ts>
class ClearAccumulatedFlowAction final : public Action<Ts...>,
                                         public Parented<UFM01Component>,
                                         public ClearAccumulatedFlowActionInterface {
 public:
  void play(const Ts &...x) override {}

  void play_complex(const Ts &...x) override {
    this->num_running_++;
    this->args_ = std::make_tuple(x...);
    this->parent_->request_clear_accumulated_flow_(this);
  }

  void complete() override {
    std::apply([this](const Ts &...x) { this->play_next_(x...); }, this->args_);
  }

 protected:
  std::tuple<Ts...> args_{};
};

}  // namespace esphome::ufm01
