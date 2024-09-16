#pragma once
#ifdef USE_OTA_STATE_CALLBACK
#include "ota_backend.h"

#include "esphome/core/automation.h"

namespace esphome {
namespace ota {

class OTAStateChangeTrigger : public Trigger<OTAState> {
 public:
  explicit OTAStateChangeTrigger(OTAComponent *parent) {
    parent->add_on_state_callback([this, parent](OTAState state, float progress, uint8_t error) {
      if (!parent->is_failed()) {
        return trigger(state);
      }
    });
  }
};

class OTAStartTrigger : public Trigger<> {
 public:
  explicit OTAStartTrigger(OTAComponent *parent) {
    parent->add_on_state_callback([this, parent](OTAState state, float progress, uint8_t error) {
      if (state == OTA_STARTED && !parent->is_failed()) {
        trigger();
      }
    });
  }
};

class OTAProgressTrigger : public Trigger<float> {
 public:
  explicit OTAProgressTrigger(OTAComponent *parent) {
    parent->add_on_state_callback([this, parent](OTAState state, float progress, uint8_t error) {
      if (state == OTA_IN_PROGRESS && !parent->is_failed()) {
        trigger(progress);
      }
    });
  }
};

class OTAEndTrigger : public Trigger<> {
 public:
  explicit OTAEndTrigger(OTAComponent *parent) {
    parent->add_on_state_callback([this, parent](OTAState state, float progress, uint8_t error) {
      if (state == OTA_COMPLETED && !parent->is_failed()) {
        trigger();
      }
    });
  }
};

class OTAAbortTrigger : public Trigger<> {
 public:
  explicit OTAAbortTrigger(OTAComponent *parent) {
    parent->add_on_state_callback([this, parent](OTAState state, float progress, uint8_t error) {
      if (state == OTA_ABORT && !parent->is_failed()) {
        trigger();
      }
    });
  }
};

class OTAErrorTrigger : public Trigger<uint8_t> {
 public:
  explicit OTAErrorTrigger(OTAComponent *parent) {
    parent->add_on_state_callback([this, parent](OTAState state, float progress, uint8_t error) {
      if (state == OTA_ERROR && !parent->is_failed()) {
        trigger(error);
      }
    });
  }
};

}  // namespace ota
}  // namespace esphome
#endif
