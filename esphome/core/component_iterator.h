#pragma once

#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/core/helpers.h"

#ifdef USE_CAMERA
#include "esphome/components/camera/camera.h"
#endif

namespace esphome {

#ifdef USE_API_USER_DEFINED_ACTIONS
namespace api {
class UserServiceDescriptor;
}  // namespace api
#endif

#ifdef USE_INFRARED
namespace infrared {
class Infrared;
}  // namespace infrared
#endif
#ifdef USE_RADIO_FREQUENCY
namespace radio_frequency {
class RadioFrequency;
}  // namespace radio_frequency
#endif

class ComponentIterator {
 public:
  void begin(bool include_internal = false);
  void advance();
  bool completed() const { return this->state_ == IteratorState::NONE; }
  virtual bool on_begin();
// Pure virtual entity callbacks (generated from entity_types.h)
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ENTITY_TYPE_(type, singular, plural, count, upper) virtual bool on_##singular(type *obj) = 0;
#define ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) \
  ENTITY_TYPE_(type, singular, plural, count, upper)
#include "esphome/core/entity_types.h"
#undef ENTITY_TYPE_
#undef ENTITY_CONTROLLER_TYPE_
// NOLINTEND(bugprone-macro-parentheses)
// Non-entity and non-pure-virtual callbacks (have default implementations)
#ifdef USE_API_USER_DEFINED_ACTIONS
  virtual bool on_service(api::UserServiceDescriptor *service);
#endif
#ifdef USE_CAMERA
  virtual bool on_camera(camera::Camera *camera);
#endif
  virtual bool on_end();

 protected:
  // Iterates over all ESPHome entities (sensors, switches, lights, etc.)
  // Supports up to 256 entity types and up to 65,535 entities of each type
  enum class IteratorState : uint8_t {
    NONE = 0,
    BEGIN,
// Entity iterator states (generated from entity_types.h)
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ENTITY_TYPE_(type, singular, plural, count, upper) upper,
#define ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) upper,
#include "esphome/core/entity_types.h"
#undef ENTITY_TYPE_
#undef ENTITY_CONTROLLER_TYPE_
// NOLINTEND(bugprone-macro-parentheses)
#ifdef USE_API_USER_DEFINED_ACTIONS
    SERVICE,
#endif
#ifdef USE_CAMERA
    CAMERA,
#endif
    MAX,
  };
  uint16_t at_{0};  // Supports up to 65,535 entities per type
  IteratorState state_{IteratorState::NONE};
  bool include_internal_{false};

  template<typename Container>
  void process_platform_item_(const Container &items,
                              bool (ComponentIterator::*on_item)(typename Container::value_type)) {
    if (this->at_ >= items.size()) {
      this->advance_platform_();
    } else {
      typename Container::value_type item = items[this->at_];
      if ((item->is_internal() && !this->include_internal_) || (this->*on_item)(item)) {
        this->at_++;
      }
    }
  }

  void advance_platform_();
};

}  // namespace esphome
