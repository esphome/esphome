#pragma once

// Included once by the generated main.cpp, after it returns the registered controllers as a tuple:
//
//   static auto esphome_controllers() { return std::tuple{api_apiserver_id, web_server_webserver_id}; }
//   #include "esphome/core/controller_dispatch.h"
//
// Defines ControllerRegistry::notify_*() as direct calls on those controllers. Excluded from
// esphome.h so nothing else includes it.

#include "esphome/core/controller_registry.h"

#ifdef USE_CONTROLLER_REGISTRY

#include <tuple>
#include <type_traits>

namespace esphome {

template<typename... Controllers> constexpr bool controllers_satisfy_contract(std::tuple<Controllers...> *) {
  static_assert((ControllerContract<std::remove_pointer_t<Controllers>> && ...),
                "A registered controller is missing an on_*_update() callback for an entity type in this build "
                "(esphome/core/controller_registry.h)");
  return true;
}
static_assert(controllers_satisfy_contract(static_cast<decltype(esphome_controllers()) *>(nullptr)));

// NOLINTBEGIN(bugprone-macro-parentheses)
#define ENTITY_TYPE_(type, singular, plural, count, upper)  // no controller callback
#define ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) \
  void ControllerRegistry::notify_##callback(type *obj) { \
    std::apply([obj](auto *...controller) { (controller->on_##callback(obj), ...); }, esphome_controllers()); \
  }
#include "esphome/core/entity_types.h"
#undef ENTITY_TYPE_
#undef ENTITY_CONTROLLER_TYPE_
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace esphome

#endif  // USE_CONTROLLER_REGISTRY
