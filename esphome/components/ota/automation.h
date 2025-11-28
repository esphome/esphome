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

class OTAStartTrigger final : public Trigger<>, public OTAStateListener {
 public:
  explicit OTAStartTrigger(OTAComponent *parent) { parent->add_state_listener(this); }

  void on_ota_state(OTAState state, float progress, uint8_t error) override {
    if (state == OTA_STARTED) {
      this->trigger();
    }
  }
};

class OTAProgressTrigger final : public Trigger<float>, public OTAStateListener {
 public:
  explicit OTAProgressTrigger(OTAComponent *parent) { parent->add_state_listener(this); }

  void on_ota_state(OTAState state, float progress, uint8_t error) override {
    if (state == OTA_IN_PROGRESS) {
      this->trigger(progress);
    }
  }
};

class OTAEndTrigger final : public Trigger<>, public OTAStateListener {
 public:
  explicit OTAEndTrigger(OTAComponent *parent) { parent->add_state_listener(this); }

  void on_ota_state(OTAState state, float progress, uint8_t error) override {
    if (state == OTA_COMPLETED) {
      this->trigger();
    }
  }
};

class OTAAbortTrigger final : public Trigger<>, public OTAStateListener {
 public:
  explicit OTAAbortTrigger(OTAComponent *parent) { parent->add_state_listener(this); }

  void on_ota_state(OTAState state, float progress, uint8_t error) override {
    if (state == OTA_ABORT) {
      this->trigger();
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
