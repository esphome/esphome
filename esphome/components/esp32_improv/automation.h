#pragma once
#ifdef USE_ESP32
#ifdef USE_ESP32_IMPROV_STATE_CALLBACK
#include "esp32_improv_component.h"

#include "esphome/core/automation.h"

#include <improv.h>

namespace esphome::esp32_improv {

class ESP32ImprovProvisionedTrigger : public Trigger<> {
 public:
  explicit ESP32ImprovProvisionedTrigger(ESP32ImprovComponent *parent) : parent_(parent) {
    parent->add_on_state_callback([this](improv::State state, improv::Error error) {
      if (state == improv::STATE_PROVISIONED && !this->parent_->is_failed()) {
        this->trigger();
      }
    });
  }

 protected:
  ESP32ImprovComponent *parent_;
};

class ESP32ImprovProvisioningTrigger : public Trigger<> {
 public:
  explicit ESP32ImprovProvisioningTrigger(ESP32ImprovComponent *parent) : parent_(parent) {
    parent->add_on_state_callback([this](improv::State state, improv::Error error) {
      if (state == improv::STATE_PROVISIONING && !this->parent_->is_failed()) {
        this->trigger();
      }
    });
  }

 protected:
  ESP32ImprovComponent *parent_;
};

class ESP32ImprovStartTrigger : public Trigger<> {
 public:
  explicit ESP32ImprovStartTrigger(ESP32ImprovComponent *parent) : parent_(parent) {
    parent->add_on_state_callback([this](improv::State state, improv::Error error) {
      if ((state == improv::STATE_AUTHORIZED || state == improv::STATE_AWAITING_AUTHORIZATION) &&
          !this->parent_->is_failed()) {
        this->trigger();
      }
    });
  }

 protected:
  ESP32ImprovComponent *parent_;
};

class ESP32ImprovStateTrigger : public Trigger<improv::State, improv::Error> {
 public:
  explicit ESP32ImprovStateTrigger(ESP32ImprovComponent *parent) : parent_(parent) {
    parent->add_on_state_callback([this](improv::State state, improv::Error error) {
      if (!this->parent_->is_failed()) {
        this->trigger(state, error);
      }
    });
  }

 protected:
  ESP32ImprovComponent *parent_;
};

class ESP32ImprovStoppedTrigger : public Trigger<> {
 public:
  explicit ESP32ImprovStoppedTrigger(ESP32ImprovComponent *parent) : parent_(parent) {
    parent->add_on_state_callback([this](improv::State state, improv::Error error) {
      if (state == improv::STATE_STOPPED && !this->parent_->is_failed()) {
        this->trigger();
      }
    });
  }

 protected:
  ESP32ImprovComponent *parent_;
};

}  // namespace esphome::esp32_improv

#endif
#endif
