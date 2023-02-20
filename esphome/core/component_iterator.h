#pragma once

#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/core/helpers.h"
#include <vector>

#ifdef USE_ESP32_CAMERA
#include "esphome/components/esp32_camera/esp32_camera.h"
#endif

namespace esphome {

#ifdef USE_API
namespace api {
class UserServiceDescriptor;
}  // namespace api
#endif

class ComponentIterator {
 public:
  void begin(bool include_internal = false);
  void advance();
  virtual bool on_begin();
#ifdef USE_API
  virtual bool on_service(api::UserServiceDescriptor *service);
#endif
#ifdef USE_ESP32_CAMERA
  virtual bool on_camera(esp32_camera::ESP32Camera *camera);
#endif

  template<typename Func> void on_entity_callback(Func &&fn) {
    using Entity = typename std::remove_pointer<typename std::tuple_element<0, arguments_t<Func>>::type>::type;
    static_assert(std::is_base_of<EntityBase, Entity>::value, "Only EntityBase subclasses can be used");
    // it is done that way to allow adding custom components without changing core
    if (callbacks_.size() < Entity::ENTITY_TYPE + 1) {
      callbacks_.resize(Entity::ENTITY_TYPE + 1);
    }
    auto cb = [fn](EntityBase *entity) -> bool { return fn(static_cast<Entity *>(entity)); };
    callbacks_[Entity::ENTITY_TYPE] = cb;
  }

  virtual bool on_end();

 protected:
  enum class IteratorState {
    NONE = 0,
    BEGIN,
    ALL_ENTITIES,
#ifdef USE_API
    SERVICE,
#endif
#ifdef USE_ESP32_CAMERA
    CAMERA,
#endif
    MAX,
  } state_{IteratorState::NONE};
  size_t at_{0};
  bool include_internal_{false};
  std::vector<std::function<bool(EntityBase *)>> callbacks_;
};

}  // namespace esphome
