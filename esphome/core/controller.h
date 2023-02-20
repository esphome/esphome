#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/application.h"

namespace esphome {

class Controller {
  template<typename... X> struct AutoLambda;
  template<typename First, typename... Args> struct AutoLambda<std::function<void(First, Args...)>> {
    template<typename Func, typename Obj> static std::function<void(Args...)> make_lambda(Func &&func, Obj *obj) {
      return [obj, func](Args... args) { func(obj, args...); };
    }
  };

 public:
  void setup_controller(bool include_internal = false);

  template<typename Func> void add_on_state_callback(Func &&fn) {
    using Entity = typename std::remove_pointer<typename std::tuple_element<0, arguments_t<Func>>::type>::type;
    static_assert(std::is_base_of<EntityBase, Entity>::value, "Only EntityBase subclasses can be used");
    for (auto &obj : App.get_entities<Entity>()) {
      if (include_internal_ || !obj->is_internal()) {
        obj->add_on_state_callback(AutoLambda<callback_t<Func>>::make_lambda(fn, obj));
      }
    }
  }

  template<typename Func> void add_new_remote_values_callback(Func &&fn) {
    using Entity = typename std::remove_pointer<typename std::tuple_element<0, arguments_t<Func>>::type>::type;
    static_assert(std::is_base_of<EntityBase, Entity>::value, "Only EntityBase subclasses can be used");
    for (auto &obj : App.get_entities<Entity>()) {
      if (include_internal_ || !obj->is_internal()) {
        obj->add_new_remote_values_callback(AutoLambda<callback_t<Func>>::make_lambda(fn, obj));
      }
    }
  }

 protected:
  bool include_internal_{false};
};

}  // namespace esphome
