#pragma once

#include "esphome/core/defines.h"
#ifdef USE_WEBSERVER
#include "esphome/core/component.h"
#include "esphome/core/component_iterator.h"
namespace esphome::json {
class JsonBuilder;
}  // namespace esphome::json

#ifdef USE_ESP32
namespace esphome::web_server_idf {
class AsyncEventSourceResponse;
}  // namespace esphome::web_server_idf
#endif

namespace esphome::web_server {

// The /events session the iterator lists entities to
#ifdef USE_ESP32
using EventSession = web_server_idf::AsyncEventSourceResponse;
#elif defined(USE_ARDUINO)
class DeferredUpdateEventSource;
using EventSession = DeferredUpdateEventSource;
#endif
class WebServer;

class ListEntitiesIterator final : public ComponentIterator {
 public:
  ListEntitiesIterator(const WebServer *ws, EventSession *session) : web_server_(ws), session_(session) {}

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

 protected:
  // Same type as message_generator_t, which this header cannot include
  void send_all_(void *source, void (*generator)(WebServer *, void *, json::JsonBuilder &));

  const WebServer *web_server_;
  EventSession *session_;
};

}  // namespace esphome::web_server
#endif
