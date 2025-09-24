#pragma once

#include "micro_wake_word.h"
#include "streaming_model.h"

#ifdef USE_ESP_IDF
namespace esphome {
namespace micro_wake_word {

template<typename... Ts> class StartAction : public Action<Ts...>, public Parented<MicroWakeWord> {
 public:
  void play(Ts... x) override { this->parent_->start(); }
};

template<typename... Ts> class StopAction : public Action<Ts...>, public Parented<MicroWakeWord> {
 public:
  void play(Ts... x) override { this->parent_->stop(); }
};

template<typename... Ts> class IsRunningCondition : public Condition<Ts...>, public Parented<MicroWakeWord> {
 public:
  bool check(Ts... x) override { return this->parent_->is_running(); }
};

template<typename... Ts> class EnableModelAction : public Action<Ts...> {
 public:
  explicit EnableModelAction(WakeWordModel *wake_word_model) : wake_word_model_(wake_word_model) {}
  void play(Ts... x) override { this->wake_word_model_->enable(); }

 protected:
  WakeWordModel *wake_word_model_;
};

template<typename... Ts> class DisableModelAction : public Action<Ts...> {
 public:
  explicit DisableModelAction(WakeWordModel *wake_word_model) : wake_word_model_(wake_word_model) {}
  void play(Ts... x) override { this->wake_word_model_->disable(); }

 protected:
  WakeWordModel *wake_word_model_;
};

template<typename... Ts> class ModelIsEnabledCondition : public Condition<Ts...> {
 public:
  explicit ModelIsEnabledCondition(WakeWordModel *wake_word_model) : wake_word_model_(wake_word_model) {}
  bool check(Ts... x) override { return this->wake_word_model_->is_enabled(); }

 protected:
  WakeWordModel *wake_word_model_;
};

}  // namespace micro_wake_word
}  // namespace esphome
#endif
