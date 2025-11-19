#pragma once

#ifdef USE_ESP_IDF

#include "modem_component.h"

#include "esphome/core/automation.h"

namespace esphome {
namespace modem {

// Triggers
class ModemOnNotRespondingTrigger : public Trigger<> {
 public:
  explicit ModemOnNotRespondingTrigger(ModemComponent *parent) {
    parent->add_on_state_callback([this, parent](ModemComponentState old_state, ModemComponentState state) {
      if (!parent->is_failed() && state == ModemComponentState::NOT_RESPONDING) {
        this->trigger();
      }
    });
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
        // only trigger if old_state is CONNECTED
        if (old_state == ModemComponentState::CONNECTED) {
          this->trigger();
        }
      }
    });
  }
};

class ModemOnPowerOnTrigger : public Trigger<> {
 public:
  explicit ModemOnPowerOnTrigger(ModemComponent *parent) {
    parent->add_on_state_callback([this, parent](ModemComponentState old_state, ModemComponentState state) {
      if (!parent->is_failed() && state == ModemComponentState::POWERING_ON) {
        this->trigger();
      }
    });
  }
};

class ModemOnEnableTrigger : public Trigger<> {
 public:
  explicit ModemOnEnableTrigger(ModemComponent *parent) {
    parent->add_on_state_callback([this, parent](ModemComponentState old_state, ModemComponentState state) {
      if (!parent->is_failed() && state == ModemComponentState::ENABLING) {
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

template<typename... Ts> class ModemResetAction : public Action<Ts...> {
 public:
  void play(const Ts &...x) override { global_modem_component->reset(); }
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
#endif  // USE_ESP_IDF
