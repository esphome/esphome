#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "../rs485_frame.h"

#include <cstring>

namespace esphome::rs485_frame {

/// Diagnostic text_sensor that publishes the most recent validated frame type as a
/// 4-character hex string ("0083" etc.).
///
/// A PollingComponent rather than a loop()-driven Component: on a live bus the last frame
/// type changes on nearly every frame (keep-alive, LED and display frames interleave; a
/// master polls several devices in turn), so publishing whenever it differs from the last
/// publish still publishes at the frame rate. update() keeps the publish-only-on-change
/// filter, run once per update_interval: instead of once per loop iteration.
class RS485FrameTextSensor : public text_sensor::TextSensor, public PollingComponent {
 public:
  void set_parent(RS485FrameHub *parent) { this->parent_ = parent; }

  void update() override {
    if (this->parent_ == nullptr)
      return;
    const char *current = this->parent_->get_last_frame_type();
    if (current[0] == '\0')
      return;  // no frames received yet
    if (std::strcmp(current, this->last_published_) == 0)
      return;  // unchanged
    std::strncpy(this->last_published_, current, sizeof(this->last_published_) - 1);
    this->last_published_[sizeof(this->last_published_) - 1] = '\0';
    this->publish_state(this->last_published_);
  }

 protected:
  RS485FrameHub *parent_{nullptr};
  // Mirror of the hub's last_frame_type_ buffer so an unchanged value is detected without
  // calling text_sensor::publish_state(), which would construct a std::string per poll.
  char last_published_[5]{};
};

}  // namespace esphome::rs485_frame
