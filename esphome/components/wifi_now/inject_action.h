
#pragma once

#include <utility>

#include "esphome/core/defines.h"
#include "esphome/core/automation.h"
#include "esphome/core/base_automation.h"

#include "simple_types.h"
#include "payload_setter.h"

namespace esphome {
namespace wifi_now {

template<typename... Ts> class WifiNowInjectAction : public Action<Ts...> {
 public:
  WifiNowInjectAction(){};

  void set_event_trigger(WifiNowBinarySensorEvent event, Trigger<> *trigger) {
    eventtriggers_[event] = [trigger]() { trigger->trigger(); };
  }

  void set_payload_setter(WifiNowTemplatePayloadSetter<WifiNowBinarySensorEvent> *payload_setter) {
    payload_setter_ = payload_setter;
  };

  void play(Ts... x) override {
    if (payload_setter_) {
      auto value = payload_setter_->get_value();
      if (value < WifiNowBinarySensorEvent::COUNT) {
        if (eventtriggers_[value]) {
          eventtriggers_[value]();
        }
      }
    }
  };

 protected:
  std::function<void()> eventtriggers_[WifiNowBinarySensorEvent::COUNT];
  WifiNowTemplatePayloadSetter<WifiNowBinarySensorEvent> *payload_setter_{nullptr};
};

}  // namespace wifi_now
}  // namespace esphome
