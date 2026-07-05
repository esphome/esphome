#pragma once

#include "esphome/core/defines.h"
#ifdef USE_WEBSERVER
#include "esphome/core/component.h"
#include "esphome/core/component_iterator.h"
namespace esphome::web_server_idf {
#ifdef USE_ESP32
class AsyncEventSource;
#endif
}  // namespace esphome::web_server_idf

namespace esphome::web_server {

#if !defined(USE_ESP32) && defined(USE_ARDUINO)
class DeferredUpdateEventSource;
#endif
class WebServer;

class ListEntitiesIterator final : public ComponentIterator {
 public:
#ifdef USE_ESP32
  ListEntitiesIterator(const WebServer *ws, esphome::web_server_idf::AsyncEventSource *es);
#elif defined(USE_ARDUINO)
  ListEntitiesIterator(const WebServer *ws, DeferredUpdateEventSource *es);
#endif

// Entity overrides (generated from entity_types.h).
// Implementations live in list_entities.cpp.
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ENTITY_TYPE_(type, singular, plural, count, upper) bool on_##singular(type *obj) override;
#define ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) \
  ENTITY_TYPE_(type, singular, plural, count, upper)
#include "esphome/core/entity_types.h"
#undef ENTITY_TYPE_
#undef ENTITY_CONTROLLER_TYPE_
  // NOLINTEND(bugprone-macro-parentheses)
  bool completed() { return this->state_ == IteratorState::NONE; }

 protected:
  const WebServer *web_server_;
#ifdef USE_ESP32
  esphome::web_server_idf::AsyncEventSource *events_;
#elif USE_ARDUINO
  DeferredUpdateEventSource *events_;
#endif
};

}  // namespace esphome::web_server
#endif
