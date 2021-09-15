#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sprinkler/sprinkler.h"

namespace esphome {
namespace sprinkler {

template<typename... Ts> class StartFullCycleAction : public Action<Ts...> {
 public:
  explicit StartFullCycleAction(Sprinkler *a_sprinkler) : sprinkler(a_sprinkler) {}

  void play(Ts... x) override { this->sprinkler->start_full_cycle(); }

 protected:
  Sprinkler *sprinkler;
};

template<typename... Ts> class ShutdownAction : public Action<Ts...> {
 public:
  explicit ShutdownAction(Sprinkler *a_sprinkler) : sprinkler(a_sprinkler) {}

  void play(Ts... x) override { this->sprinkler->shutdown(); }

 protected:
  Sprinkler *sprinkler;
};

template<typename... Ts> class NextValveAction : public Action<Ts...> {
 public:
  explicit NextValveAction(Sprinkler *a_sprinkler) : sprinkler(a_sprinkler) {}

  void play(Ts... x) override { this->sprinkler->next_valve(); }

 protected:
  Sprinkler *sprinkler;
};

template<typename... Ts> class PreviousValveAction : public Action<Ts...> {
 public:
  explicit PreviousValveAction(Sprinkler *a_sprinkler) : sprinkler(a_sprinkler) {}

  void play(Ts... x) override { this->sprinkler->previous_valve(); }

 protected:
  Sprinkler *sprinkler;
};

template<typename... Ts> class PauseAction : public Action<Ts...> {
 public:
  explicit PauseAction(Sprinkler *a_sprinkler) : sprinkler(a_sprinkler) {}

  void play(Ts... x) override { this->sprinkler->pause(); }

 protected:
  Sprinkler *sprinkler;
};

template<typename... Ts> class ResumeAction : public Action<Ts...> {
 public:
  explicit ResumeAction(Sprinkler *a_sprinkler) : sprinkler(a_sprinkler) {}

  void play(Ts... x) override { this->sprinkler->resume(); }

 protected:
  Sprinkler *sprinkler;
};

template<typename... Ts> class ResumeOrStartAction : public Action<Ts...> {
 public:
  explicit ResumeOrStartAction(Sprinkler *a_sprinkler) : sprinkler(a_sprinkler) {}

  void play(Ts... x) override { this->sprinkler->resume_or_start_full_cycle(); }

 protected:
  Sprinkler *sprinkler;
};

}  // namespace sprinkler
}  // namespace esphome
