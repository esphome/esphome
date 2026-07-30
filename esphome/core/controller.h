#pragma once

#include "esphome/core/entity_includes.h"

namespace esphome {

class Controller {
 public:
// Controller virtual methods (generated from entity_types.h)
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ENTITY_TYPE_(type, singular, plural, count, upper)  // no controller callback
#define ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) virtual void on_##callback(type *obj){};
#include "esphome/core/entity_types.h"
#undef ENTITY_TYPE_
#undef ENTITY_CONTROLLER_TYPE_
  // NOLINTEND(bugprone-macro-parentheses)
};

}  // namespace esphome
