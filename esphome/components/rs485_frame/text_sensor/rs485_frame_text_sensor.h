#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "../rs485_frame.h"

#include <cstring>

namespace esphome::rs485_frame {

/// Diagnostic text_sensor that publishes the most recent validated frame type as a
/// 4-character hex string ("0083" etc.). Publishes only on change; the publish_state
/// implementation in text_sensor::TextSensor internally deduplicates so a steady-state
/// bus produces no API traffic.
class RS485FrameTextSensor : public text_sensor::TextSensor, public Component {
 public:
  void set_parent(RS485FrameHub *parent) { this->parent_ = parent; }

  void loop() override {
    const char *current = this->parent_->get_last_frame_type();
    if (current[0] == '\0')
      return;  // no frames received yet
    if (std::strcmp(current, this->last_published_) == 0)
      return;  // unchanged
    std::strncpy(this->last_published_, current, sizeof(this->last_published_) - 1);
    this->last_published_[sizeof(this->last_published_) - 1] = '\0';
    this->publish_state(this->last_published_);
  }
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  RS485FrameHub *parent_{nullptr};
  // Mirror of the hub's last_frame_type_ buffer so we can detect changes without
  // calling text_sensor::publish_state() (which would still dedupe, but at the cost
  // of constructing a std::string per loop iteration).
  char last_published_[5]{};
};

}  // namespace esphome::rs485_frame
