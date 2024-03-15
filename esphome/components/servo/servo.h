#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace servo {

extern uint32_t global_servo_id;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

class Servo : public Component {
 public:
  void set_output(output::FloatOutput *output) { output_ = output; }
  void loop() override;
  void write(float value);
  void internal_write(float value);
  void detach();
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_min_level(float min_level) { min_level_ = min_level; }
  void set_idle_level(float idle_level) { idle_level_ = idle_level; }
  void set_max_level(float max_level) { max_level_ = max_level; }
  void set_restore(bool restore) { restore_ = restore; }
  void set_auto_detach_time(uint32_t auto_detach_time) { auto_detach_time_ = auto_detach_time; }
  void set_transition_length(uint32_t transition_length) { transition_length_ = transition_length; }

  bool has_reached_target() { return this->current_value_ == this->target_value_; }

 protected:
  void save_level_(float v);

  output::FloatOutput *output_;
  float min_level_ = 0.0300f;
  float idle_level_ = 0.0750f;
  float max_level_ = 0.1200f;
  bool restore_{false};
  uint32_t auto_detach_time_ = 0;
  uint32_t transition_length_ = 0;
  ESPPreferenceObject rtc_;
  uint8_t state_;
  float target_value_ = 0;
  float source_value_ = 0;
  float current_value_ = 0;
  uint32_t start_millis_ = 0;
  enum State {
    STATE_ATTACHED = 0,
    STATE_DETACHED = 1,
    STATE_TARGET_REACHED = 2,
  };
};

template<typename... Ts> class ServoWriteAction : public Action<Ts...> {
 public:
  ServoWriteAction(Servo *servo) : servo_(servo) {}
  TEMPLATABLE_VALUE(float, value)

  void play(Ts... x) override { this->servo_->write(this->value_.value(x...)); }

 protected:
  Servo *servo_;
};

template<typename... Ts> class ServoDetachAction : public Action<Ts...> {
 public:
  ServoDetachAction(Servo *servo) : servo_(servo) {}

  void play(Ts... x) override { this->servo_->detach(); }

 protected:
  Servo *servo_;
};

}  // namespace servo
}  // namespace esphome
