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

bool ComponentIterator::advance_step_() {
  switch (this->state_) {
    case IteratorState::NONE:
      // not started
      return false;
    case IteratorState::BEGIN:
      if (this->on_begin()) {
        advance_platform_();
        return true;
      }
      return false;

// Entity iterator cases (generated from entity_types.h)
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ENTITY_TYPE_(type, singular, plural, count, upper) \
  case IteratorState::upper: \
    return this->process_platform_item_(App.get_##plural(), &ComponentIterator::on_##singular);
#define ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) \
  ENTITY_TYPE_(type, singular, plural, count, upper)
#include "esphome/core/entity_types.h"
#undef ENTITY_TYPE_
#undef ENTITY_CONTROLLER_TYPE_
      // NOLINTEND(bugprone-macro-parentheses)

#ifdef USE_API_USER_DEFINED_ACTIONS
    case IteratorState::SERVICE:
      return this->process_platform_item_(api::global_api_server->get_user_services(), &ComponentIterator::on_service);
#endif

#ifdef USE_CAMERA
    case IteratorState::CAMERA: {
      camera::Camera *camera_instance = camera::Camera::instance();
      if (camera_instance != nullptr && (!camera_instance->is_internal() || this->include_internal_) &&
          !this->on_camera(camera_instance)) {
        return false;
      }
      advance_platform_();
      return true;
    }
#endif

    case IteratorState::MAX:
      if (this->on_end()) {
        this->state_ = IteratorState::NONE;
        return true;
      }
      return false;
  }
  return false;
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
