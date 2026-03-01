#pragma once
#ifdef USE_OTA_STATE_LISTENER
#include "ota_backend.h"

#include "esphome/core/automation.h"

namespace esphome {
namespace ota {

class OTAStateChangeTrigger final : public Trigger<OTAState>, public OTAStateListener {
 public:
  explicit OTAStateChangeTrigger(OTAComponent *parent) : parent_(parent) { parent->add_state_listener(this); }

  void on_ota_state(OTAState state, float progress, uint8_t error) override {
    if (!this->parent_->is_failed()) {
      this->trigger(state);
    }
  }

 protected:
  OTAComponent *parent_;
};

template<OTAState State> class OTAStateTrigger final : public Trigger<>, public OTAStateListener {
 public:
  explicit OTAStateTrigger(OTAComponent *parent) : parent_(parent) { parent->add_state_listener(this); }

  void on_ota_state(OTAState state, float progress, uint8_t error) override {
    if (state == State && !this->parent_->is_failed()) {
      this->trigger();
    }
  }

 protected:
  OTAComponent *parent_;
};

using OTAStartTrigger = OTAStateTrigger<OTA_STARTED>;
using OTAEndTrigger = OTAStateTrigger<OTA_COMPLETED>;
using OTAAbortTrigger = OTAStateTrigger<OTA_ABORT>;

class OTAProgressTrigger final : public Trigger<float>, public OTAStateListener {
 public:
  explicit OTAProgressTrigger(OTAComponent *parent) : parent_(parent) { parent->add_state_listener(this); }

  void on_ota_state(OTAState state, float progress, uint8_t error) override {
    if (state == OTA_IN_PROGRESS && !this->parent_->is_failed()) {
      this->trigger(progress);
    }
  }

 protected:
  OTAComponent *parent_;
};

class OTAErrorTrigger final : public Trigger<uint8_t>, public OTAStateListener {
 public:
  explicit OTAErrorTrigger(OTAComponent *parent) : parent_(parent) { parent->add_state_listener(this); }

  void on_ota_state(OTAState state, float progress, uint8_t error) override {
    if (state == OTA_ERROR && !this->parent_->is_failed()) {
      this->trigger(error);
    }
  }

 protected:
  OTAComponent *parent_;
};

}  // namespace ota
}  // namespace esphome
#endif
