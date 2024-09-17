#pragma once
#ifdef USE_ESP32
#include "esp32_improv_component.h"

#include "esphome/core/automation.h"

#include <improv.h>

namespace esphome {
namespace esp32_improv {

class ESP32ImprovAuthorizedTrigger : public Trigger<> {
 public:
  explicit ESP32ImprovAuthorizedTrigger(ESP32ImprovComponent *parent) {
    parent->add_on_state_callback([this, parent](improv::State state) {
      if (state == improv::STATE_AUTHORIZED && !parent->is_failed()) {
        trigger();
      }
    });
  }
};

class ESP32ImprovAwaitingAuthorizationTrigger : public Trigger<> {
 public:
  explicit ESP32ImprovAwaitingAuthorizationTrigger(ESP32ImprovComponent *parent) {
    parent->add_on_state_callback([this, parent](improv::State state) {
      if (state == improv::STATE_AWAITING_AUTHORIZATION && !parent->is_failed()) {
        trigger();
      }
    });
  }
};

class ESP32ImprovProvisionedTrigger : public Trigger<> {
 public:
  explicit ESP32ImprovProvisionedTrigger(ESP32ImprovComponent *parent) {
    parent->add_on_state_callback([this, parent](improv::State state) {
      if (state == improv::STATE_PROVISIONED && !parent->is_failed()) {
        trigger();
      }
    });
  }
};

class ESP32ImprovProvisioningTrigger : public Trigger<> {
 public:
  explicit ESP32ImprovProvisioningTrigger(ESP32ImprovComponent *parent) {
    parent->add_on_state_callback([this, parent](improv::State state) {
      if (state == improv::STATE_PROVISIONING && !parent->is_failed()) {
        trigger();
      }
    });
  }
};

class ESP32ImprovStateChangeTrigger : public Trigger<improv::State> {
 public:
  explicit ESP32ImprovStateChangeTrigger(ESP32ImprovComponent *parent) {
    parent->add_on_state_callback([this, parent](improv::State state) {
      if (!parent->is_failed()) {
        trigger(state);
      }
    });
  }
};

class ESP32ImprovStoppedTrigger : public Trigger<> {
 public:
  explicit ESP32ImprovStoppedTrigger(ESP32ImprovComponent *parent) {
    parent->add_on_state_callback([this, parent](improv::State state) {
      if (state == improv::STATE_STOPPED && !parent->is_failed()) {
        trigger();
      }
    });
  }
};

}  // namespace esp32_improv
}  // namespace esphome
#endif
