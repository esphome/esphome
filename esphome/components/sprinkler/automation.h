#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/sprinkler/sprinkler.h"

namespace esphome {
namespace sprinkler {

template<typename... Ts> class SetMultiplierAction : public Action<Ts...> {
 public:
  explicit SetMultiplierAction(Sprinkler *a_sprinkler) : sprinkler_(a_sprinkler) {}

  TEMPLATABLE_VALUE(float, multiplier)

  void play(Ts... x) override { this->sprinkler_->set_multiplier(this->multiplier_.optional_value(x...)); }

 protected:
  Sprinkler *sprinkler_;
};

template<typename... Ts> class StartFullCycleAction : public Action<Ts...> {
 public:
  explicit StartFullCycleAction(Sprinkler *a_sprinkler) : sprinkler_(a_sprinkler) {}

  void play(Ts... x) override { this->sprinkler_->start_full_cycle(); }

 protected:
  Sprinkler *sprinkler_;
};

template<typename... Ts> class StartSingleValveAction : public Action<Ts...> {
 public:
  explicit StartSingleValveAction(Sprinkler *a_sprinkler) : sprinkler_(a_sprinkler) {}

  TEMPLATABLE_VALUE(size_t, valve_to_start)

  void play(Ts... x) override { this->sprinkler_->start_single_valve(this->valve_to_start_.optional_value(x...)); }

 protected:
  Sprinkler *sprinkler_;
};

template<typename... Ts> class ShutdownAction : public Action<Ts...> {
 public:
  explicit ShutdownAction(Sprinkler *a_sprinkler) : sprinkler_(a_sprinkler) {}

  void play(Ts... x) override { this->sprinkler_->shutdown(); }

 protected:
  Sprinkler *sprinkler_;
};

template<typename... Ts> class NextValveAction : public Action<Ts...> {
 public:
  explicit NextValveAction(Sprinkler *a_sprinkler) : sprinkler_(a_sprinkler) {}

  void play(Ts... x) override { this->sprinkler_->next_valve(); }

 protected:
  Sprinkler *sprinkler_;
};

template<typename... Ts> class PreviousValveAction : public Action<Ts...> {
 public:
  explicit PreviousValveAction(Sprinkler *a_sprinkler) : sprinkler_(a_sprinkler) {}

  void play(Ts... x) override { this->sprinkler_->previous_valve(); }

 protected:
  Sprinkler *sprinkler_;
};

template<typename... Ts> class PauseAction : public Action<Ts...> {
 public:
  explicit PauseAction(Sprinkler *a_sprinkler) : sprinkler_(a_sprinkler) {}

  void play(Ts... x) override { this->sprinkler_->pause(); }

 protected:
  Sprinkler *sprinkler_;
};

template<typename... Ts> class ResumeAction : public Action<Ts...> {
 public:
  explicit ResumeAction(Sprinkler *a_sprinkler) : sprinkler_(a_sprinkler) {}

  void play(Ts... x) override { this->sprinkler_->resume(); }

 protected:
  Sprinkler *sprinkler_;
};

template<typename... Ts> class ResumeOrStartAction : public Action<Ts...> {
 public:
  explicit ResumeOrStartAction(Sprinkler *a_sprinkler) : sprinkler_(a_sprinkler) {}

  void play(Ts... x) override { this->sprinkler_->resume_or_start_full_cycle(); }

 protected:
  Sprinkler *sprinkler_;
};

}  // namespace sprinkler
}  // namespace esphome
