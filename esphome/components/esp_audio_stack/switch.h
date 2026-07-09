#pragma once

#if defined(USE_ESP32) && defined(USE_SWITCH)

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "esp_audio_stack.h"
#include "audio_core_log_utils.h"

namespace esphome::esp_audio_stack {

class AECSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(ESPAudioStack *parent) { this->parent_ = parent; }

  void setup() override {
    if (this->parent_ == nullptr)
      return;

    auto initial = this->get_initial_state_with_restore_mode();
    if (initial.has_value()) {
      this->write_state(*initial);
    } else {
      this->publish_state(this->parent_->is_processor_enabled());
    }
  }

  void dump_config() override { log_config("audio_stack.aec_switch", "AEC Switch"); }

 protected:
  void write_state(bool state) override {
    if (this->parent_ != nullptr) {
      this->parent_->set_processor_enabled(state);
      this->publish_state(state);
    }
  }

  ESPAudioStack *parent_{nullptr};
};

}  // namespace esphome::esp_audio_stack

#endif  // USE_ESP32 && USE_SWITCH
