#pragma once

// Included once by the generated main.cpp, after it returns the registered controllers as a tuple:
//
//   static auto esphome_controllers() { return std::tuple{api_apiserver_id, web_server_webserver_id}; }
//   #include "esphome/core/controller_dispatch.h"
//
// Defines ControllerRegistry::notify_*() as direct calls on those controllers. Excluded from
// esphome.h and the clang-tidy all-headers file, so nothing else includes it.

#include <tuple>
#include <type_traits>

#include "esphome/core/controller_registry.h"

namespace esphome {

// NOLINTBEGIN(bugprone-macro-parentheses)

/// A controller provides a plain on_*_update() member for every entity type in the build.
template<typename T>
concept ControllerContract = requires(T &controller) {
  controller;  // keeps the requirement list non-empty when no entity type has a callback
#define ENTITY_TYPE_(type, singular, plural, count, upper)  // no controller callback
#define ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) \
  controller.on_##callback(static_cast<type *>(nullptr));
#include "esphome/core/entity_types.h"
#undef ENTITY_TYPE_
#undef ENTITY_CONTROLLER_TYPE_
};

template<typename... Controllers> constexpr bool controllers_satisfy_contract(std::tuple<Controllers...> *) {
  return (ControllerContract<std::remove_pointer_t<Controllers>> && ...);
}
static_assert(controllers_satisfy_contract(static_cast<decltype(esphome_controllers()) *>(nullptr)),
              "A registered controller is missing an on_*_update() callback for an entity type in this build "
              "(ControllerContract in esphome/core/controller_dispatch.h)");

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
