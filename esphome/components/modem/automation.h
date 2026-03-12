#pragma once

#ifdef USE_ESP32

#include "modem_component.h"

#include "esphome/core/automation.h"

namespace esphome {
namespace modem {

// Triggers
class ModemOnNotRespondingTrigger : public Trigger<> {
 public:
  explicit ModemOnNotRespondingTrigger(ModemComponent *parent) {
    parent->add_on_state_callback([this, parent](ModemComponentState state) {
      if (!parent->is_failed() && state == ModemComponentState::MODEM_NOT_RESPONDING) {
        this->trigger();
      }
    });
  }
};

class ModemOnConnectTrigger : public Trigger<> {
 public:
  explicit ModemOnConnectTrigger(ModemComponent *parent) {
    parent->add_on_state_callback([this, parent](ModemComponentState state) {
      if (!parent->is_failed() && state == ModemComponentState::MODEM_CONNECTED) {
        this->trigger();
      }
    });
  }
};

class ModemOnDisconnectTrigger : public Trigger<> {
 public:
  explicit ModemOnDisconnectTrigger(ModemComponent *parent) {
    parent->add_on_state_callback([this, parent](ModemComponentState state) {
      if (!parent->is_failed() && state == ModemComponentState::MODEM_DISCONNECTED) {
        this->trigger();
      }
    });
  }
};

class ModemOnEnableTrigger : public Trigger<> {
 public:
  explicit ModemOnEnableTrigger(ModemComponent *parent) {
    parent->add_on_state_callback([this, parent](ModemComponentState state) {
      if (!parent->is_failed() && state == ModemComponentState::MODEM_ENABLING) {
        this->trigger();
      }
    });
  }
};

class ModemOnDisableTrigger : public Trigger<> {
 public:
  explicit ModemOnDisableTrigger(ModemComponent *parent) {
    parent->add_on_state_callback([this, parent](ModemComponentState state) {
      if (!parent->is_failed() && state == ModemComponentState::MODEM_DISABLED) {
        this->trigger();
      }
    });
  }
};

class ModemOnSyncTrigger : public Trigger<> {
 public:
  explicit ModemOnSyncTrigger(ModemComponent *parent) {
    parent->add_on_state_callback([this, parent](ModemComponentState state) {
      if (!parent->is_failed() && state == ModemComponentState::MODEM_SYNCED) {
        this->trigger();
      }
    });
  }
};

template<typename... Ts> class ModemSendAtAction : public Action<Ts...> {
 public:
  void set_command(const std::string &command) { this->command_ = command; }
  void play(const Ts &...x) override {
    if (global_modem_component) {
      global_modem_component->send_at(this->command_, 1000, true);
    }
  }

 protected:
  std::string command_;
};

// Actions
template<typename... Ts> class ModemEnableAction : public Action<Ts...> {
 public:
  void play(const Ts &...x) override { global_modem_component->enable(); }
};

template<typename... Ts> class ModemDisableAction : public Action<Ts...> {
 public:
  void play(const Ts &...x) override { global_modem_component->disable(); }
};

// Conditions
template<typename... Ts> class ModemConnectedCondition : public Condition<Ts...> {
 public:
  bool check(const Ts &...x) override {
    if (global_modem_component) {
      return global_modem_component->is_connected();
    }
    return false;
  }
};

template<typename... Ts> class ModemEnabledCondition : public Condition<Ts...> {
 public:
  bool check(const Ts &...x) override {
    if (global_modem_component) {
      return !global_modem_component->is_disabled();
    }
    return false;
  }
};

}  // namespace modem
}  // namespace esphome
#endif  // USE_ESP32
