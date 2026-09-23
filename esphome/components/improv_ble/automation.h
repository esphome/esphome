#pragma once
#ifdef USE_ESP32
#ifdef USE_IMPROV_BLE_STATE_CALLBACK
#include "improv_ble_component.h"

#include "esphome/core/automation.h"

#include <improv.h>

namespace esphome::improv_ble {

class ImprovBLEProvisionedTrigger final : public Trigger<> {
 public:
  explicit ImprovBLEProvisionedTrigger(ImprovBLEComponent *parent) : parent_(parent) {
    parent->add_on_state_callback([this](improv::State state, improv::Error error) {
      if (state == improv::STATE_PROVISIONED && !this->parent_->is_failed()) {
        this->trigger();
      }
    });
  }

 protected:
  ImprovBLEComponent *parent_;
};

class ImprovBLEProvisioningTrigger final : public Trigger<> {
 public:
  explicit ImprovBLEProvisioningTrigger(ImprovBLEComponent *parent) : parent_(parent) {
    parent->add_on_state_callback([this](improv::State state, improv::Error error) {
      if (state == improv::STATE_PROVISIONING && !this->parent_->is_failed()) {
        this->trigger();
      }
    });
  }

 protected:
  ImprovBLEComponent *parent_;
};

class ImprovBLEStartTrigger final : public Trigger<> {
 public:
  explicit ImprovBLEStartTrigger(ImprovBLEComponent *parent) : parent_(parent) {
    parent->add_on_state_callback([this](improv::State state, improv::Error error) {
      if ((state == improv::STATE_AUTHORIZED || state == improv::STATE_AWAITING_AUTHORIZATION) &&
          !this->parent_->is_failed()) {
        this->trigger();
      }
    });
  }

 protected:
  ImprovBLEComponent *parent_;
};

class ImprovBLEStateTrigger final : public Trigger<improv::State, improv::Error> {
 public:
  explicit ImprovBLEStateTrigger(ImprovBLEComponent *parent) : parent_(parent) {
    parent->add_on_state_callback([this](improv::State state, improv::Error error) {
      if (!this->parent_->is_failed()) {
        this->trigger(state, error);
      }
    });
  }

 protected:
  ImprovBLEComponent *parent_;
};

class ImprovBLEStoppedTrigger final : public Trigger<> {
 public:
  explicit ImprovBLEStoppedTrigger(ImprovBLEComponent *parent) : parent_(parent) {
    parent->add_on_state_callback([this](improv::State state, improv::Error error) {
      if (state == improv::STATE_STOPPED && !this->parent_->is_failed()) {
        this->trigger();
      }
    });
  }

 protected:
  ImprovBLEComponent *parent_;
};

}  // namespace esphome::improv_ble

#endif
#endif
