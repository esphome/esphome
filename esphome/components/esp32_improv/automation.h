#pragma once
#ifdef USE_ESP32
#ifdef USE_ESP32_IMPROV_STATE_CALLBACK
#include "esp32_improv_component.h"

#include "esphome/core/automation.h"

#include <improv.h>

namespace esphome {
namespace esp32_improv {

class ESP32ImprovProvisionedTrigger : public Trigger<> {
 public:
  explicit ESP32ImprovProvisionedTrigger(ESP32ImprovComponent *parent) {
    parent->add_on_state_callback([this, parent](improv::State state, improv::Error error) {
      if (state == improv::STATE_PROVISIONED && !parent->is_failed()) {
        trigger();
      }
    });
  }
};

class ESP32ImprovProvisioningTrigger : public Trigger<> {
 public:
  explicit ESP32ImprovProvisioningTrigger(ESP32ImprovComponent *parent) {
    parent->add_on_state_callback([this, parent](improv::State state, improv::Error error) {
      if (state == improv::STATE_PROVISIONING && !parent->is_failed()) {
        trigger();
      }
    });
  }
};

class ESP32ImprovStartTrigger : public Trigger<> {
 public:
  explicit ESP32ImprovStartTrigger(ESP32ImprovComponent *parent) {
    parent->add_on_state_callback([this, parent](improv::State state, improv::Error error) {
      if ((state == improv::STATE_AUTHORIZED || state == improv::STATE_AWAITING_AUTHORIZATION) &&
          !parent->is_failed()) {
        trigger();
      }
    });
  }
};

class ESP32ImprovStateTrigger : public Trigger<improv::State, improv::Error> {
 public:
  explicit ESP32ImprovStateTrigger(ESP32ImprovComponent *parent) {
    parent->add_on_state_callback([this, parent](improv::State state, improv::Error error) {
      if (!parent->is_failed()) {
        trigger(state, error);
      }
    });
  }
};

class ESP32ImprovStoppedTrigger : public Trigger<> {
 public:
  explicit ESP32ImprovStoppedTrigger(ESP32ImprovComponent *parent) {
    parent->add_on_state_callback([this, parent](improv::State state, improv::Error error) {
      if (state == improv::STATE_STOPPED && !parent->is_failed()) {
        trigger();
      }
    });
  }
};

}  // namespace esp32_improv
}  // namespace esphome
#endif
#endif
