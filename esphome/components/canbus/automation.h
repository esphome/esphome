#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/canbus/canbus.h"

namespace esphome {
namespace canbus {

template<typename... Ts> class SendAction : public Action<Ts...> {
 public:
  explicit SendAction(Canbus *a_canbus, int can_id) : canbus_(a_canbus), can_id_(can_id) { }

  void play(Ts... x) override { 
    uint8_t data[8]={0};
    data[1] = 10;
    this->canbus_->send(this->can_id_,data); }

 protected:
  Canbus *canbus_;
  int can_id_;
};

template<typename... Ts> class SendStateAction : public Action<Ts...> {
 public:
  SendStateAction(Canbus *a_canbus) : canbus_(a_canbus) {}
  TEMPLATABLE_VALUE(bool, state)
  void play(Ts... x) override { this->canbus_->send_state(this->state_.value(x...)); }

 protected:
  Canbus *canbus_;
};

}  // namespace canbus
}  // namespace esphome
