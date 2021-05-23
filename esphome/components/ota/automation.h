#pragma once
#ifdef USE_OTA_STATE_CALLBACK

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/ota/ota_component.h"

namespace esphome {
namespace ota {

class OTAStartTrigger : public Trigger<> {
 public:
  explicit OTAStartTrigger(OTAComponent *parent) {
    parent->add_on_state_callback([this, parent](OTAState state, float progress, uint8_t error) {
      if (state == OTAState::STARTED && !parent->is_failed()) {
        trigger();
      }
    });
  }
};

class OTAProgressTrigger : public Trigger<float> {
 public:
  explicit OTAProgressTrigger(OTAComponent *parent) {
    parent->add_on_state_callback([this, parent](OTAState state, float progress, uint8_t error) {
      if (state == OTAState::IN_PROGRESS && !parent->is_failed()) {
        trigger(progress);
      }
    });
  }
};

class OTAEndTrigger : public Trigger<> {
 public:
  explicit OTAEndTrigger(OTAComponent *parent) {
    parent->add_on_state_callback([this, parent](OTAState state, float progress, uint8_t error) {
      if (state == OTAState::COMPLETED && !parent->is_failed()) {
        trigger();
      }
    });
  }
};


class OTAErrorTrigger : public Trigger<int> {
 public:
  explicit OTAErrorTrigger(OTAComponent *parent) {
    parent->add_on_state_callback([this, parent](OTAState state, float progress, uint8_t error) {
      if (state == OTAState::ERROR && !parent->is_failed()) {
        trigger(error);
      }
    });
  }
};

}  // namespace ota
}  // namespace esphome

#endif // USE_OTA_STATE_CALLBACK
