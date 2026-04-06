#pragma once

#include "esphome/core/automation.h"
#include "speaker.h"

#include <vector>

namespace esphome {
namespace speaker {

template<typename... Ts> class PlayAction : public Action<Ts...>, public Parented<Speaker> {
 public:
  void set_data_template(std::vector<uint8_t> (*func)(Ts...)) {
    this->data_.func = func;
    this->len_ = -1;  // Sentinel value indicates template mode
  }

  void set_data_static(const uint8_t *data, size_t len) {
    this->data_.data = data;
    this->len_ = len;  // Length >= 0 indicates static mode
  }

  void play(const Ts &...x) override {
    if (this->len_ >= 0) {
      // Static mode: pass pointer directly to play(const uint8_t *, size_t)
      this->parent_->play(this->data_.data, static_cast<size_t>(this->len_));
    } else {
      // Template mode: call function and pass vector to play(const std::vector<uint8_t> &)
      auto val = this->data_.func(x...);
      this->parent_->play(val);
    }
  }

 protected:
  ssize_t len_{-1};  // -1 = template mode, >=0 = static mode with length
  union Data {
    std::vector<uint8_t> (*func)(Ts...);  // Function pointer (stateless lambdas)
    const uint8_t *data;                  // Pointer to static data in flash
  } data_;
};

template<typename... Ts> class VolumeSetAction : public Action<Ts...>, public Parented<Speaker> {
  TEMPLATABLE_VALUE(float, volume)
  void play(const Ts &...x) override { this->parent_->set_volume(this->volume_.value(x...)); }
};

template<typename... Ts> class MuteOnAction : public Action<Ts...> {
 public:
  explicit MuteOnAction(Speaker *speaker) : speaker_(speaker) {}

  void play(const Ts &...x) override { this->speaker_->set_mute_state(true); }

 protected:
  Speaker *speaker_;
};

template<typename... Ts> class MuteOffAction : public Action<Ts...> {
 public:
  explicit MuteOffAction(Speaker *speaker) : speaker_(speaker) {}

  void play(const Ts &...x) override { this->speaker_->set_mute_state(false); }

 protected:
  Speaker *speaker_;
};

template<typename... Ts> class StopAction : public Action<Ts...>, public Parented<Speaker> {
 public:
  void play(const Ts &...x) override { this->parent_->stop(); }
};

template<typename... Ts> class FinishAction : public Action<Ts...>, public Parented<Speaker> {
 public:
  void play(const Ts &...x) override { this->parent_->finish(); }
};

template<typename... Ts> class IsPlayingCondition : public Condition<Ts...>, public Parented<Speaker> {
 public:
  bool check(const Ts &...x) override { return this->parent_->is_running(); }
};

template<typename... Ts> class IsStoppedCondition : public Condition<Ts...>, public Parented<Speaker> {
 public:
  bool check(const Ts &...x) override { return this->parent_->is_stopped(); }
};

}  // namespace speaker
}  // namespace esphome
