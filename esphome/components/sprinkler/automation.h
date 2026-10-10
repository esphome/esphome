#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/sprinkler/sprinkler.h"

namespace esphome::sprinkler {

template<typename... Ts> class StartSingleValveAction final : public Action<Ts...> {
 public:
  explicit StartSingleValveAction(Sprinkler *a_sprinkler) : sprinkler_(a_sprinkler) {}

  // TemplatableValue (not TemplatableFn) — also set from C++ with raw values in sprinkler.cpp
  template<typename V> void set_valve_to_start(V valve_to_start) { this->valve_to_start_ = valve_to_start; }
  TEMPLATABLE_VALUE(uint32_t, valve_run_duration)

  void play(const Ts &...x) override {
    this->sprinkler_->start_single_valve(this->valve_to_start_.optional_value(x...),
                                         this->valve_run_duration_.optional_value(x...));
  }

 protected:
  Sprinkler *sprinkler_;
  TemplatableValue<size_t, Ts...> valve_to_start_{};
};

template<typename... Ts> class ShutdownAction final : public Action<Ts...> {
 public:
  explicit ShutdownAction(Sprinkler *a_sprinkler) : sprinkler_(a_sprinkler) {}

  void play(const Ts &...x) override { this->sprinkler_->shutdown(); }

 protected:
  Sprinkler *sprinkler_;
};

template<typename... Ts> class ResumeOrStartAction final : public Action<Ts...> {
 public:
  explicit ResumeOrStartAction(Sprinkler *a_sprinkler) : sprinkler_(a_sprinkler) {}

  void play(const Ts &...x) override { this->sprinkler_->resume_or_start_full_cycle(); }

 protected:
  Sprinkler *sprinkler_;
};

}  // namespace esphome::sprinkler
