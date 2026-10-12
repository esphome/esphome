#pragma once

#include "esphome/core/defines.h"

#ifdef USE_CONTROLLER_REGISTRY

#include "esphome/core/entity_includes.h"

namespace esphome {

/** Fan-out of entity state updates to the controllers (APIServer, WebServer).
 *
 * Entities call ControllerRegistry::notify_*_update() instead of holding
 * per-entity controller callbacks. The functions are only declared here;
 * controller_dispatch.h, included by the generated main.cpp, defines them as
 * direct calls on the controllers registered through CORE.register_controller().
 */
class ControllerRegistry {
 public:
// Notify method declarations (generated from entity_types.h)
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ENTITY_TYPE_(type, singular, plural, count, upper)  // no controller callback
#define ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) \
  static void notify_##callback(type *obj);
#include "esphome/core/entity_types.h"
#undef ENTITY_TYPE_
#undef ENTITY_CONTROLLER_TYPE_
  // NOLINTEND(bugprone-macro-parentheses)
};

}  // namespace esphome

#endif  // USE_CONTROLLER_REGISTRY
