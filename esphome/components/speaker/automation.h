#pragma once

#include "esphome/core/automation.h"
#include "speaker.h"

#include <vector>

namespace esphome {
namespace speaker {

template<typename... Ts> class PlayAction : public Action<Ts...>, public Parented<Speaker> {
 public:
  void set_data_template(std::function<std::vector<uint8_t>(Ts...)> func) {
    this->data_func_ = func;
    this->static_ = false;
  }
  void set_data_static(const std::vector<uint8_t> &data) {
    this->data_static_ = data;
    this->static_ = true;
  }

  void play(Ts... x) override {
    if (this->static_) {
      this->parent_->play(this->data_static_);
    } else {
      auto val = this->data_func_(x...);
      this->parent_->play(val);
    }
  }

 protected:
  bool static_{false};
  std::function<std::vector<uint8_t>(Ts...)> data_func_{};
  std::vector<uint8_t> data_static_{};
};

template<typename... Ts> class StopAction : public Action<Ts...>, public Parented<Speaker> {
 public:
  void play(Ts... x) override { this->parent_->stop(); }
};

template<typename... Ts> class FinishAction : public Action<Ts...>, public Parented<Speaker> {
 public:
  void play(Ts... x) override { this->parent_->finish(); }
};

template<typename... Ts> class IsPlayingCondition : public Condition<Ts...>, public Parented<Speaker> {
 public:
  bool check(Ts... x) override { return this->parent_->is_running(); }
};

template<typename... Ts> class IsStoppedCondition : public Condition<Ts...>, public Parented<Speaker> {
 public:
  bool check(Ts... x) override { return this->parent_->is_stopped(); }
};

}  // namespace speaker
}  // namespace esphome
