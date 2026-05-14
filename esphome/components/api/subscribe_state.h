#pragma once

#include "esphome/core/defines.h"
#ifdef USE_API
#include "esphome/core/component.h"
#include "esphome/core/component_iterator.h"
#include "esphome/core/controller.h"
namespace esphome::api {

class APIConnection;

// Macro for generating InitialStateIterator handlers
// Calls send_*_state
#define INITIAL_STATE_HANDLER(entity_type, EntityClass) \
  bool InitialStateIterator::on_##entity_type(EntityClass *entity) { /* NOLINT(bugprone-macro-parentheses) */ \
    return this->client_->send_##entity_type##_state(entity); \
  }

class InitialStateIterator final : public ComponentIterator {
 public:
  InitialStateIterator(APIConnection *client);

// Entity overrides (generated from entity_types.h).
// ENTITY_TYPE_ entities have no state to send and default to a no-op.
// ENTITY_CONTROLLER_TYPE_ entities are implemented in subscribe_state.cpp via INITIAL_STATE_HANDLER,
// except on_event which has no state (defined out-of-line in subscribe_state.cpp).
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ENTITY_TYPE_(type, singular, plural, count, upper) \
  bool on_##singular(type *entity) override { return true; }
#define ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) \
  bool on_##singular(type *entity) override;
#include "esphome/core/entity_types.h"
#undef ENTITY_TYPE_
#undef ENTITY_CONTROLLER_TYPE_
  // NOLINTEND(bugprone-macro-parentheses)

 protected:
  APIConnection *client_;
};

}  // namespace esphome::api
#endif
