#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF

#include "esphome/core/automation.h"
#include "sendspin_hub.h"

namespace esphome {
namespace sendspin {

template<typename... Ts> class SendSwitchCommandAction : public Action<Ts...>, public Parented<SendspinHub> {
 public:
  void play(const Ts &...x) override { this->parent_->send_client_command(SendspinCommandType::SWITCH); }
};

}  // namespace sendspin
}  // namespace esphome

#endif  // USE_ESP_IDF
