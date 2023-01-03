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

template<typename... Ts> class QueueValveAction : public Action<Ts...> {
 public:
  explicit QueueValveAction(Sprinkler *a_sprinkler) : sprinkler_(a_sprinkler) {}

  TEMPLATABLE_VALUE(size_t, valve_number)
  TEMPLATABLE_VALUE(uint32_t, valve_run_duration)

  void play(Ts... x) override {
    this->sprinkler_->queue_valve(this->valve_number_.optional_value(x...),
                                  this->valve_run_duration_.optional_value(x...));
  }

 protected:
  Sprinkler *sprinkler_;
};

template<typename... Ts> class ClearQueuedValvesAction : public Action<Ts...> {
 public:
  explicit ClearQueuedValvesAction(Sprinkler *a_sprinkler) : sprinkler_(a_sprinkler) {}

  void play(Ts... x) override { this->sprinkler_->clear_queued_valves(); }

 protected:
  Sprinkler *sprinkler_;
};

template<typename... Ts> class SetRepeatAction : public Action<Ts...> {
 public:
  explicit SetRepeatAction(Sprinkler *a_sprinkler) : sprinkler_(a_sprinkler) {}

  TEMPLATABLE_VALUE(uint32_t, repeat)

  void play(Ts... x) override { this->sprinkler_->set_repeat(this->repeat_.optional_value(x...)); }

 protected:
  Sprinkler *sprinkler_;
};

template<typename... Ts> class SetRunDurationAction : public Action<Ts...> {
 public:
  explicit SetRunDurationAction(Sprinkler *a_sprinkler) : sprinkler_(a_sprinkler) {}

  TEMPLATABLE_VALUE(size_t, valve_number)
  TEMPLATABLE_VALUE(uint32_t, valve_run_duration)

  void play(Ts... x) override {
    this->sprinkler_->set_valve_run_duration(this->valve_number_.optional_value(x...),
                                             this->valve_run_duration_.optional_value(x...));
  }

 protected:
  Sprinkler *sprinkler_;
};

template<typename... Ts> class StartFromQueueAction : public Action<Ts...> {
 public:
  explicit StartFromQueueAction(Sprinkler *a_sprinkler) : sprinkler_(a_sprinkler) {}

  void play(Ts... x) override { this->sprinkler_->start_from_queue(); }

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
