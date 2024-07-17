#pragma once

#ifdef USE_ESP_IDF

#include "modem_component.h"

#include "esphome/core/automation.h"

namespace esphome {
namespace modem {

class ModemOnNotRespondingTrigger : public Trigger<> {
 public:
  explicit ModemOnNotRespondingTrigger(ModemComponent *parent) {
    parent->add_on_state_callback([this, parent](ModemState state) {
      if (!parent->is_failed() && state == ModemState::NOT_RESPONDING) {
        this->trigger();
      }
    });
  }
};

class ModemOnConnectTrigger : public Trigger<> {
 public:
  explicit ModemOnConnectTrigger(ModemComponent *parent) {
    parent->add_on_state_callback([this, parent](ModemState state) {
      if (!parent->is_failed() && state == ModemState::CONNECTED) {
        this->trigger();
      }
    });
  }
};

}  // namespace modem
}  // namespace esphome
#endif  // USE_ESP_IDF
