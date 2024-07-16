#pragma once
#include "modem_component.h"

#include "esphome/core/automation.h"

namespace esphome {
namespace modem {

class ModemNotRespondingTrigger : public Trigger<> {
 public:
  explicit ModemNotRespondingTrigger(ModemComponent *parent) {
    parent->add_on_not_responding_callback([this, parent]() { this->trigger(); });
  }
};

}  // namespace modem
}  // namespace esphome
