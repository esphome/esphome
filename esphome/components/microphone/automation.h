#pragma once

#include "esphome/core/automation.h"
#include "microphone.h"

#include <vector>

namespace esphome::microphone {

template<typename... Ts> class CaptureAction final : public Action<Ts...>, public Parented<Microphone> {
  void play(const Ts &...x) override { this->parent_->start(); }
};

template<typename... Ts> class StopCaptureAction final : public Action<Ts...>, public Parented<Microphone> {
  void play(const Ts &...x) override { this->parent_->stop(); }
};

template<typename... Ts> class MuteAction final : public Action<Ts...>, public Parented<Microphone> {
  void play(const Ts &...x) override { this->parent_->set_mute_state(true); }
};
template<typename... Ts> class UnmuteAction final : public Action<Ts...>, public Parented<Microphone> {
  void play(const Ts &...x) override { this->parent_->set_mute_state(false); }
};

class DataTrigger final : public Trigger<const std::vector<uint8_t> &> {
 public:
  explicit DataTrigger(Microphone *mic) {
    mic->add_data_callback([this](const std::vector<uint8_t> &data) { this->trigger(data); });
  }
};

template<typename... Ts> class IsCapturingCondition final : public Condition<Ts...>, public Parented<Microphone> {
 public:
  bool check(const Ts &...x) override { return this->parent_->is_running(); }
};

template<typename... Ts> class IsMutedCondition final : public Condition<Ts...>, public Parented<Microphone> {
 public:
  bool check(const Ts &...x) override { return this->parent_->get_mute_state(); }
};

}  // namespace esphome::microphone
