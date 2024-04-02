#pragma once

#ifdef USE_OTA_STATE_CALLBACK
#include "ota_esphome.h"

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace ota_esphome {

class OTAESPHomeStateChangeTrigger : public Trigger<OTAESPHomeState> {
 public:
  explicit OTAESPHomeStateChangeTrigger(OTAESPHomeComponent *parent) {
    parent->add_on_state_callback([this, parent](OTAESPHomeState state, float progress, uint8_t error) {
      if (!parent->is_failed()) {
        return trigger(state);
      }
    });
  }
};

class OTAESPHomeStartTrigger : public Trigger<> {
 public:
  explicit OTAESPHomeStartTrigger(OTAESPHomeComponent *parent) {
    parent->add_on_state_callback([this, parent](OTAESPHomeState state, float progress, uint8_t error) {
      if (state == OTA_STARTED && !parent->is_failed()) {
        trigger();
      }
    });
  }
};

class OTAESPHomeProgressTrigger : public Trigger<float> {
 public:
  explicit OTAESPHomeProgressTrigger(OTAESPHomeComponent *parent) {
    parent->add_on_state_callback([this, parent](OTAESPHomeState state, float progress, uint8_t error) {
      if (state == OTA_IN_PROGRESS && !parent->is_failed()) {
        trigger(progress);
      }
    });
  }
};

class OTAESPHomeEndTrigger : public Trigger<> {
 public:
  explicit OTAESPHomeEndTrigger(OTAESPHomeComponent *parent) {
    parent->add_on_state_callback([this, parent](OTAESPHomeState state, float progress, uint8_t error) {
      if (state == OTA_COMPLETED && !parent->is_failed()) {
        trigger();
      }
    });
  }
};

class OTAESPHomeErrorTrigger : public Trigger<uint8_t> {
 public:
  explicit OTAESPHomeErrorTrigger(OTAESPHomeComponent *parent) {
    parent->add_on_state_callback([this, parent](OTAESPHomeState state, float progress, uint8_t error) {
      if (state == OTA_ERROR && !parent->is_failed()) {
        trigger(error);
      }
    });
  }
};

}  // namespace ota_esphome
}  // namespace esphome

#endif  // USE_OTA_STATE_CALLBACK
