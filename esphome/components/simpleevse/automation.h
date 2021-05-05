#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/simpleevse/simplesevse.h"

namespace esphome {
namespace simpleevse {

template<typename... Ts> class PluggedCondition : public Condition<Ts...> {
 public:
  PluggedCondition(SimpleEvseComponent *parent) : parent_(parent) {}
  bool check(Ts... x) override { 
    uint16_t vehicle_state = this->parent_->get_register()[REGISTER_VEHICLE_STATE];
    return vehicle_state == VehicleState::VEHICLE_EV_PRESENT || vehicle_state == VehicleState::VEHICLE_CHARGING || vehicle_state == VehicleState::VEHICLE_CHARGING_WITH_VENT;
}

 protected:
  SimpleEvseComponent *parent_;
};


template<typename... Ts> class UnpluggedCondition : public Condition<Ts...> {
 public:
  UnpluggedCondition(SimpleEvseComponent *parent) : parent_(parent) {}
  bool check(Ts... x) override { 
    uint16_t vehicle_state = this->parent_->get_register()[REGISTER_VEHICLE_STATE];
    return vehicle_state == VehicleState::VEHICLE_READY;
}

 protected:
  SimpleEvseComponent *parent_;
};


}   // namespace simple_evse
}   // namespace esphome