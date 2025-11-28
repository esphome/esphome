#pragma once
#ifdef USE_OTA_STATE_LISTENER
#include "ota_backend.h"

#include "esphome/core/automation.h"

namespace esphome {
namespace ota {

class OTAStateChangeTrigger final : public Trigger<OTAState>, public OTAStateListener {
 public:
  explicit OTAStateChangeTrigger(OTAComponent *parent) { parent->add_state_listener(this); }

  void on_ota_state(OTAState state, float progress, uint8_t error) override { this->trigger(state); }
};

template<OTAState State> class OTAStateTrigger final : public Trigger<>, public OTAStateListener {
 public:
  explicit OTAStateTrigger(OTAComponent *parent) { parent->add_state_listener(this); }

  void on_ota_state(OTAState state, float progress, uint8_t error) override {
    if (state == State) {
      this->trigger();
    }
  }
};

using OTAStartTrigger = OTAStateTrigger<OTA_STARTED>;
using OTAEndTrigger = OTAStateTrigger<OTA_COMPLETED>;
using OTAAbortTrigger = OTAStateTrigger<OTA_ABORT>;

class OTAProgressTrigger final : public Trigger<float>, public OTAStateListener {
 public:
  explicit OTAProgressTrigger(OTAComponent *parent) { parent->add_state_listener(this); }

  void on_ota_state(OTAState state, float progress, uint8_t error) override {
    if (state == OTA_IN_PROGRESS) {
      this->trigger(progress);
    }
  }
};

class OTAErrorTrigger final : public Trigger<uint8_t>, public OTAStateListener {
 public:
  explicit OTAErrorTrigger(OTAComponent *parent) { parent->add_state_listener(this); }

  void on_ota_state(OTAState state, float progress, uint8_t error) override {
    if (state == OTA_ERROR) {
      this->trigger(error);
    }
  }
};

}  // namespace ota
}  // namespace esphome
#endif
