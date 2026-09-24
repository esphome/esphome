#pragma once

#include "esphome/core/defines.h"

#ifdef USE_CONTROLLER_REGISTRY

#include "esphome/core/entity_includes.h"

namespace esphome {

/// A controller provides a plain on_*_update() member for every entity type in the build.
/// Checked by a static_assert in the generated main.cpp for each registered controller.
// NOLINTBEGIN(bugprone-macro-parentheses)
template<typename T>
concept ControllerContract = requires(T &controller) {
  controller;
#define ENTITY_TYPE_(type, singular, plural, count, upper)  // no controller callback
#define ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) \
  controller.on_##callback(static_cast<type *>(nullptr));
#include "esphome/core/entity_types.h"
#undef ENTITY_TYPE_
#undef ENTITY_CONTROLLER_TYPE_
};
// NOLINTEND(bugprone-macro-parentheses)

/** Fan-out of entity state updates to the controllers (APIServer, WebServer).
 *
 * Entities call ControllerRegistry::notify_*_update() instead of holding
 * per-entity controller callbacks. The notify functions are only declared here;
 * code generation defines them in main.cpp as direct calls on each controller
 * that registered through CORE.register_controller(), so there is no virtual
 * dispatch, no controller base class and no runtime list of controllers.
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
