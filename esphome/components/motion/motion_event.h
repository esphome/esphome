#pragma once

#include "esphome/core/component.h"
#include "esphome/components/event/event.h"
#include "motion_component.h"

namespace esphome::motion {

class MotionEvent : public Component, public event::Event {
 public:
  explicit MotionEvent(MotionComponent *parent);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_threshold(float threshold) { this->threshold_ = threshold; }
  void set_cooldown(uint32_t cooldown) { this->cooldown_ = cooldown; }

 protected:
  void process_motion_data_(const MotionData &data);

  MotionComponent *parent_;
  float threshold_{0.5f};
  uint32_t cooldown_{500};

  // State tracking for shake detection
  uint32_t last_trigger_time_{0};
  float last_accel_[3]{NAN, NAN, NAN};
};

}  // namespace esphome::motion
