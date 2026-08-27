#pragma once

#include "esphome/core/defines.h"
#ifdef USE_API
#include "esphome/core/component.h"
#include "esphome/core/component_iterator.h"
namespace esphome::api {

class APIConnection;

// Macro for generating ListEntitiesIterator handlers
// Calls schedule_message_ which dispatches to try_send_*_info
#define LIST_ENTITIES_HANDLER(entity_type, EntityClass, ResponseType) \
  bool ListEntitiesIterator::on_##entity_type(EntityClass *entity) { /* NOLINT(bugprone-macro-parentheses) */ \
    return this->client_->schedule_message_(entity, ResponseType::MESSAGE_TYPE, ResponseType::ESTIMATED_SIZE); \
  }

class ListEntitiesIterator final : public ComponentIterator {
 public:
  ListEntitiesIterator(APIConnection *client);

// Entity overrides (generated from entity_types.h).
// All implementations live in list_entities.cpp via LIST_ENTITIES_HANDLER.
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ENTITY_TYPE_(type, singular, plural, count, upper) bool on_##singular(type *entity) override;
#define ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) \
  ENTITY_TYPE_(type, singular, plural, count, upper)
#include "esphome/core/entity_types.h"
#undef ENTITY_TYPE_
#undef ENTITY_CONTROLLER_TYPE_
  // NOLINTEND(bugprone-macro-parentheses)
#ifdef USE_API_USER_DEFINED_ACTIONS
  bool on_service(UserServiceDescriptor *service) override;
#endif
#ifdef USE_CAMERA
  bool on_camera(camera::Camera *entity) override;
#endif
  bool on_end() override;

 protected:
  APIConnection *client_;
};

}  // namespace esphome::api
#endif
