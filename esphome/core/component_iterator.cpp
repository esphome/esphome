#include "component_iterator.h"

#include "esphome/core/application.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif
#ifdef USE_API_USER_DEFINED_ACTIONS
#include "esphome/components/api/user_services.h"
#endif

namespace esphome {

void ComponentIterator::begin(bool include_internal) {
  this->state_ = IteratorState::BEGIN;
  this->at_ = 0;
  this->include_internal_ = include_internal;
}

void ComponentIterator::advance_platform_() {
  this->state_ = static_cast<IteratorState>(static_cast<uint32_t>(this->state_) + 1);
  this->at_ = 0;
}

void ComponentIterator::advance() {
  switch (this->state_) {
    case IteratorState::NONE:
      // not started
      return;
    case IteratorState::BEGIN:
      if (this->on_begin()) {
        advance_platform_();
      }
      break;

// Entity iterator cases (generated from entity_types.h)
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ENTITY_TYPE_(type, singular, plural, count, upper) \
  case IteratorState::upper: \
    this->process_platform_item_(App.get_##plural(), &ComponentIterator::on_##singular); \
    break;
#define ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) \
  ENTITY_TYPE_(type, singular, plural, count, upper)
#include "esphome/core/entity_types.h"
#undef ENTITY_TYPE_
#undef ENTITY_CONTROLLER_TYPE_
      // NOLINTEND(bugprone-macro-parentheses)

#ifdef USE_API_USER_DEFINED_ACTIONS
    case IteratorState::SERVICE:
      this->process_platform_item_(api::global_api_server->get_user_services(), &ComponentIterator::on_service);
      break;
#endif

#ifdef USE_CAMERA
    case IteratorState::CAMERA: {
      camera::Camera *camera_instance = camera::Camera::instance();
      if (camera_instance != nullptr && (!camera_instance->is_internal() || this->include_internal_)) {
        this->on_camera(camera_instance);
      }
      advance_platform_();
    } break;
#endif

    case IteratorState::MAX:
      if (this->on_end()) {
        this->state_ = IteratorState::NONE;
      }
      return;
  }
}

bool ComponentIterator::on_end() { return true; }
bool ComponentIterator::on_begin() { return true; }
#ifdef USE_API_USER_DEFINED_ACTIONS
bool ComponentIterator::on_service(api::UserServiceDescriptor *service) { return true; }
#endif
#ifdef USE_CAMERA
bool ComponentIterator::on_camera(camera::Camera *camera) { return true; }
#endif
}  // namespace esphome
