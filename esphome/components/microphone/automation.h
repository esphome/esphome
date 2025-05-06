#pragma once

#include "esphome/core/automation.h"
#include "microphone.h"

#include <vector>

namespace esphome {
namespace microphone {

template<typename... Ts> class CaptureAction : public Action<Ts...>, public Parented<Microphone> {
  void play(Ts... x) override { this->parent_->start(); }
};

template<typename... Ts> class StopCaptureAction : public Action<Ts...>, public Parented<Microphone> {
  void play(Ts... x) override { this->parent_->stop(); }
};

template<typename... Ts> class MuteAction : public Action<Ts...>, public Parented<Microphone> {
  void play(Ts... x) override { this->parent_->set_mute_state(true); }
};
template<typename... Ts> class UnmuteAction : public Action<Ts...>, public Parented<Microphone> {
  void play(Ts... x) override { this->parent_->set_mute_state(false); }
};

class DataTrigger : public Trigger<const std::vector<uint8_t> &> {
 public:
  explicit DataTrigger(Microphone *mic) {
    mic->add_data_callback([this](const std::vector<uint8_t> &data) { this->trigger(data); });
  }
};

template<typename... Ts> class IsCapturingCondition : public Condition<Ts...>, public Parented<Microphone> {
 public:
  bool check(Ts... x) override { return this->parent_->is_running(); }
};

template<typename... Ts> class IsMutedCondition : public Condition<Ts...>, public Parented<Microphone> {
 public:
  bool check(Ts... x) override { return this->parent_->get_mute_state(); }
};

}  // namespace microphone
}  // namespace esphome
