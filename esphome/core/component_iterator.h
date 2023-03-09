#pragma once

#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/core/helpers.h"

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
  template<typename Tuple> struct entityCallbacks;
  template<typename... Args> struct entityCallbacks<std::tuple<Args...>> {
    using type = std::tuple<std::function<bool(Args)>...>;
  };

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
    using Entity = typename std::tuple_element<0, arguments_t<Func>>::type;
    get_by_type<callback_t<Func>>(callbacks_) = [this, fn](Entity entity) {
      if (entity->is_internal() && !include_internal_) {
        return true;
      }
      return fn(entity);
    };
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
  entityCallbacks<entities_t>::type callbacks_;
};

}  // namespace esphome
