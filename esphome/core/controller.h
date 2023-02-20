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
  template<typename Func> static void add_on_state_callback(Func &&fn, bool include_internal = false) {
    using Entity = typename std::remove_pointer<typename std::tuple_element<0, arguments_t<Func>>::type>::type;
    static_assert(std::is_base_of<EntityBase, Entity>::value, "Only EntityBase subclasses can be used");
    for (auto &entity : App.get_entities<Entity>()) {
      auto *obj = static_cast<Entity *>(entity);
      if (include_internal || !obj->is_internal()) {
        obj->add_on_state_callback(AutoLambda<callback_t<Func>>::make_lambda(fn, obj));
      }
    }
  }
};

}  // namespace esphome
