#pragma once

#include <cinttypes>
#include <utility>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/hal.h"
#include "esphome/core/entity_base.h"

namespace esphome {

template<typename... Ts> class EntityBaseStateCondition : public Condition<Ts...> {
 public:
  EntityBaseStateCondition(EntityBase_State *parent, bool unavailable) : parent_(parent), unavailable_(unavailable) {}
  bool check(Ts... x) override {
    if (this->unavailable_) {
      return this->parent_->is_unavailable();
    }
    return this->parent_->is_unknown();
  }

 protected:
  EntityBase_State *parent_;
  bool unavailable_;
};

class EntityBaseStateTriggerUnavailable : public Trigger<> {
 public:
  EntityBaseStateTriggerUnavailable(EntityBase_State *parent) {
    parent->add_on_state_callback([this, parent]() {
      if (parent->is_unavailable()) {
        this->trigger();
      }
    });
  }
};

class EntityBaseStateTriggerUnknown : public Trigger<> {
 public:
  EntityBaseStateTriggerUnknown(EntityBase_State *parent) {
    parent->add_on_state_callback([this, parent]() {
      if (parent->is_unknown()) {
        this->trigger();
      }
    });
  }
};

}  // namespace esphome
