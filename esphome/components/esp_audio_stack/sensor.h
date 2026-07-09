#pragma once

#if defined(USE_ESP32) && defined(USE_SENSOR)

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esp_audio_stack.h"
#include "audio_core_log_utils.h"

namespace esphome::esp_audio_stack {

class TdmSlotLevelSensor : public sensor::Sensor, public PollingComponent, public Parented<ESPAudioStack> {
 public:
  void set_slot(uint8_t slot) { this->slot_ = slot; }

  void update() override {
    if (this->parent_ != nullptr) {
      this->publish_state(this->parent_->get_tdm_slot_level_dbfs(this->slot_));
    }
  }

  void dump_config() override {
    log_config("audio_stack.tdm_slot_sensor", "ESP Audio Stack TDM Slot %u Level Sensor", this->slot_);
  }

 protected:
  uint8_t slot_{0};
};

}  // namespace esphome::esp_audio_stack

#endif  // USE_ESP32 && USE_SENSOR
