#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/canbus/canbus.h"

namespace esphome {
namespace canbus {

template<typename... Ts> class SendAction : public Action<Ts...> {
 public:
  explicit SendAction(Canbus *parent) : parent_(parent){}
  void set_can_id(int can_id) {this->can_id_ = can_id;}

  TEMPLATABLE_VALUE(float, data)

  void play(Ts... x) override { 
    auto call = this->parent_->make_call(this->can_id_);
    //unsigned uint const * p = reinterpret_cast<unsigned char const *>(&f);
    call.set_data(this->data_.optional_value(x...));
//    call.perform(this->parent_, this->can_id_);
  }
 protected:
  Canbus *parent_;
  int can_id_;
};


}  // namespace canbus
}  // namespace esphome
