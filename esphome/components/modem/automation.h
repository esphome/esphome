#pragma once

#ifdef USE_ESP_IDF

#include "modem_component.h"

#include "esphome/core/automation.h"

namespace esphome {
namespace modem {

class ModemOnNotRespondingTrigger : public Trigger<> {
  // not managed by `add_on_state_callback`, because we want to execute the callback
  // as a single mode script (we have to know when the callback has ended)
 public:
  explicit ModemOnNotRespondingTrigger(ModemComponent *parent) {
    parent->set_not_responding_cb(static_cast<Trigger<> *>(this));
  }
};

class ModemOnConnectTrigger : public Trigger<> {
 public:
  explicit ModemOnConnectTrigger(ModemComponent *parent) {
    parent->add_on_state_callback([this, parent](ModemComponentState old_state, ModemComponentState state) {
      if (!parent->is_failed() && state == ModemComponentState::CONNECTED) {
        this->trigger();
      }
    });
  }
};

class ModemOnDisconnectTrigger : public Trigger<> {
 public:
  explicit ModemOnDisconnectTrigger(ModemComponent *parent) {
    parent->add_on_state_callback([this, parent](ModemComponentState old_state, ModemComponentState state) {
      if (!parent->is_failed() && state == ModemComponentState::DISCONNECTED) {
        // filter useless old_state
        if (old_state == ModemComponentState::CONNECTED) {
          this->trigger();
        }
      }
    });
  }
};

}  // namespace modem
}  // namespace esphome
#endif  // USE_ESP_IDF
