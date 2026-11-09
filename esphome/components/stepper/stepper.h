#pragma once

#include "esphome/core/component.h"

namespace esphome::stepper {

#define LOG_STEPPER(this) \
  ESP_LOGCONFIG(TAG, \
                "  Acceleration: %.0f steps/s^2\n" \
                "  Deceleration: %.0f steps/s^2\n" \
                "  Max Speed: %.0f steps/s", \
                this->acceleration_, this->deceleration_, this->max_speed_);

class Stepper {
 public:
  void set_target(int32_t steps) { this->target_position = steps; }
  void report_position(int32_t steps) { this->current_position = steps; }
  void set_acceleration(float acceleration) { this->acceleration_ = acceleration; }
  void set_deceleration(float deceleration) { this->deceleration_ = deceleration; }
  void set_max_speed(float max_speed) { this->max_speed_ = max_speed; }
  virtual void on_update_speed() {}
  bool has_reached_target() { return this->current_position == this->target_position; }

  int32_t current_position{0};
  int32_t target_position{0};

 protected:
  void calculate_speed_(uint32_t now);
  int32_t should_step_();

  float acceleration_{1e6f};
  float deceleration_{1e6f};
  float current_speed_{0.0f};
  float max_speed_{1e6f};
  uint32_t last_calculation_{0};
  uint32_t last_step_{0};
};

}  // namespace esphome::stepper
