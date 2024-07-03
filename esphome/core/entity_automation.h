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

}  // namespace esphome
