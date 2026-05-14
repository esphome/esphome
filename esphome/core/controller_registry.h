#pragma once

#include "esphome/core/defines.h"

#ifdef USE_CONTROLLER_REGISTRY

#include "esphome/core/entity_includes.h"
#include "esphome/core/helpers.h"

namespace esphome {

class Controller;

/** Global registry for Controllers to receive entity state updates.
 *
 * This singleton registry allows Controllers (APIServer, WebServer) to receive
 * entity state change notifications without storing per-entity callbacks.
 *
 * Instead of each entity maintaining controller callbacks (32 bytes overhead per entity),
 * entities call ControllerRegistry::notify_*_update() which iterates the small list
 * of registered controllers (typically 2: API and WebServer).
 *
 * Each notify method directly iterates controllers and calls the virtual method,
 * avoiding function pointer indirection for minimal dispatch overhead.
 *
 * Memory savings: 32 bytes per entity (2 controllers × 16 bytes std::function overhead)
 * Typical config (25 entities): ~780 bytes saved
 * Large config (80 entities): ~2,540 bytes saved
 */
class ControllerRegistry {
 public:
  /** Register a controller to receive entity state updates.
   *
   * Controllers should call this in their setup() method.
   * Typically only APIServer and WebServer register.
   */
  static void register_controller(Controller *controller) { controllers.push_back(controller); }

// Notify method declarations (generated from entity_types.h)
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ENTITY_TYPE_(type, singular, plural, count, upper)  // no controller callback
#define ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) \
  static void notify_##callback(type *obj);
#include "esphome/core/entity_types.h"
#undef ENTITY_TYPE_
#undef ENTITY_CONTROLLER_TYPE_
  // NOLINTEND(bugprone-macro-parentheses)

 protected:
  static StaticVector<Controller *, CONTROLLER_REGISTRY_MAX> controllers;
};

}  // namespace esphome

// Include controller.h AFTER the class definition so notify methods can be
// defined inline. This is safe because controller_registry.h is only ever
// included from .cpp files, never from other headers.
#include "esphome/core/controller.h"

namespace esphome {

// Inline notify methods — each is a tiny loop over 1-2 controllers.
// Defining them here (rather than in controller_registry.cpp) allows the
// compiler to inline them into the single call site in each entity's
// notify_frontend_(), eliminating an unnecessary function-call frame.

// NOLINTBEGIN(bugprone-macro-parentheses)
#define ENTITY_TYPE_(type, singular, plural, count, upper)  // no controller callback
#define ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) \
  inline void ControllerRegistry::notify_##callback(type *obj) { \
    for (auto *controller : controllers) { \
      controller->on_##callback(obj); \
    } \
  }
#include "esphome/core/entity_types.h"
#undef ENTITY_TYPE_
#undef ENTITY_CONTROLLER_TYPE_
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace esphome

#endif  // USE_CONTROLLER_REGISTRY
