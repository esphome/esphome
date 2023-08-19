#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/application.h"

namespace esphome {

class Controller {
  template<typename Entity> class DiscardArg {
   public:
    DiscardArg(std::function<void(Entity *)> &&cb, Entity *entity) {
      this->cb = cb;
      this->entity = entity;
    }
    template<typename... T> void operator()(const T &...) const { cb(entity); }

   private:
    std::function<void(Entity *)> cb;
    Entity *entity;
  };

 public:
  template<typename Func> static void add_on_state_callback(Func &&fn, bool include_internal = false) {
    using Entity = typename std::remove_pointer<typename std::tuple_element<0, arguments_t<Func>>::type>::type;
    static_assert(std::is_base_of<EntityBase, Entity>::value, "Only EntityBase subclasses can be used");
    for (auto &entity : App.get_entities<Entity>()) {
      auto *obj = static_cast<Entity *>(entity);
      if (include_internal || !obj->is_internal()) {
        DiscardArg<Entity> cb(fn, obj);
        obj->add_on_state_callback(cb);
      }
    }
  }
};

}  // namespace esphome
