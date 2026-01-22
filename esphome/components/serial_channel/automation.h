#pragma once

#include "esphome/core/automation.h"
#include "serial_channel.h"

namespace esphome::serial_channel {

class SerialChannelStateTrigger : public Trigger<std::string> {
 public:
  explicit SerialChannelStateTrigger(SerialChannel *parent) {
    parent->add_on_state_callback([this](const std::string &value) { this->trigger(value); });
  }
};

template<typename... Ts> class SerialChannelSendAction : public Action<Ts...> {
 public:
  explicit SerialChannelSendAction(SerialChannel *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(std::string, data)

  void play(Ts... x) override {
    auto call = this->parent_->make_call();
    auto data = this->data_.value(x...);
    call.set_data(data);
    call.perform();
  }

 protected:
  SerialChannel *parent_;
};

}  // namespace esphome::serial_channel
