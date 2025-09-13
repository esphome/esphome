#pragma once

#include "update_entity.h"

#include "esphome/core/automation.h"

namespace esphome {
namespace update {

template<typename... Ts> class PerformAction : public Action<Ts...>, public Parented<UpdateEntity> {
  TEMPLATABLE_VALUE(bool, force)

 public:
  void play(Ts... x) override { this->parent_->perform(this->force_.value(x...)); }
};

template<typename... Ts> class IsAvailableCondition : public Condition<Ts...>, public Parented<UpdateEntity> {
 public:
  bool check(Ts... x) override { return this->parent_->state == UPDATE_STATE_AVAILABLE; }
};

}  // namespace update
}  // namespace esphome
