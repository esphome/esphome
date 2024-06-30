#pragma once

#include <cinttypes>
#include <utility>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/hal.h"
#include "homeassistant_cover.h"

namespace esphome {
namespace homeassistant {

template<typename... Ts> class CoverCondition : public Condition<Ts...> {
 public:
  CoverCondition(HomeassistantCover *parent, bool unavailable) : parent_(parent), unavailable_(unavailable) {}
  bool check(Ts... x) override {
    if (this->unavailable_) {
      return this->parent_->is_unavailable();
    }
    return this->parent_->is_unknown();
  }

 protected:
  HomeassistantCover *parent_;
  bool unavailable_;
};

}  // namespace homeassistant
}  // namespace esphome
