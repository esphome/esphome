#pragma once

#include <array>

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace rotary_encoder {

/// All possible restore modes for the rotary encoder
enum RotaryEncoderRestoreMode {
  ROTARY_ENCODER_RESTORE_DEFAULT_ZERO,  /// try to restore counter, otherwise set to zero
  ROTARY_ENCODER_ALWAYS_ZERO,           /// do not restore counter, always set to zero
};

/// All possible resolutions for the rotary encoder
enum RotaryEncoderResolution {
  ROTARY_ENCODER_1_PULSE_PER_CYCLE =
      0x4400,  /// increment counter by 1 with every A-B cycle, slow response but accurate
  ROTARY_ENCODER_2_PULSES_PER_CYCLE = 0x2200,  /// increment counter by 2 with every A-B cycle
  ROTARY_ENCODER_4_PULSES_PER_CYCLE = 0x1100,  /// increment counter by 4 with every A-B cycle, most inaccurate
};

struct RotaryEncoderSensorStore {
  ISRInternalGPIOPin pin_a;
  ISRInternalGPIOPin pin_b;

  volatile int32_t counter{0};
  RotaryEncoderResolution resolution{ROTARY_ENCODER_1_PULSE_PER_CYCLE};
  int32_t min_value{INT32_MIN};
  int32_t max_value{INT32_MAX};
  int32_t last_read{0};
  uint8_t state{0};
  bool first_read{true};

  std::array<int8_t, 8> rotation_events{};
  bool rotation_events_overflow{false};

  static void gpio_intr(RotaryEncoderSensorStore *arg);
};

class RotaryEncoderSensor : public sensor::Sensor, public Component {
 public:
  void set_pin_a(InternalGPIOPin *pin_a) { pin_a_ = pin_a; }
  void set_pin_b(InternalGPIOPin *pin_b) { pin_b_ = pin_b; }

  /** Set the restore mode of the rotary encoder.
   *
   * By default (if possible) the last known counter state is restored. Otherwise the value 0 is used.
   * Restoring the state can also be turned off.
   *
   * @param restore_mode The restore mode to use.
   */
  void set_restore_mode(RotaryEncoderRestoreMode restore_mode);

  /** Set the resolution of the rotary encoder.
   *
   * By default, this component will increment the counter by 1 with every A-B input cycle.
   * You can however change this behavior to have more coarse resolutions like 4 counter increases per A-B cycle.
   *
   * @param mode The new mode of the encoder.
   */
  void set_resolution(RotaryEncoderResolution mode);

  /// Manually set the value of the counter.
  void set_value(int value) {
    this->store_.counter = value;
    this->loop();
  }

  void set_reset_pin(GPIOPin *pin_i) { this->pin_i_ = pin_i; }
  void set_min_value(int32_t min_value);
  void set_max_value(int32_t max_value);
  void set_publish_initial_value(bool publish_initial_value) { publish_initial_value_ = publish_initial_value; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;
  void loop() override;

  float get_setup_priority() const override;

  void add_on_clockwise_callback(std::function<void()> callback) {
    this->on_clockwise_callback_.add(std::move(callback));
  }

  void add_on_anticlockwise_callback(std::function<void()> callback) {
    this->on_anticlockwise_callback_.add(std::move(callback));
  }

 protected:
  InternalGPIOPin *pin_a_;
  InternalGPIOPin *pin_b_;
  GPIOPin *pin_i_{nullptr};  /// Index pin, if this is not nullptr, the counter will reset to 0 once this pin is HIGH.
  bool publish_initial_value_;
  ESPPreferenceObject rtc_;
  RotaryEncoderRestoreMode restore_mode_{ROTARY_ENCODER_RESTORE_DEFAULT_ZERO};

  RotaryEncoderSensorStore store_{};

  CallbackManager<void()> on_clockwise_callback_;
  CallbackManager<void()> on_anticlockwise_callback_;
};

template<typename... Ts> class RotaryEncoderSetValueAction : public Action<Ts...> {
 public:
  RotaryEncoderSetValueAction(RotaryEncoderSensor *encoder) : encoder_(encoder) {}
  TEMPLATABLE_VALUE(int, value)

  void play(Ts... x) override { this->encoder_->set_value(this->value_.value(x...)); }

 protected:
  RotaryEncoderSensor *encoder_;
};

class RotaryEncoderClockwiseTrigger : public Trigger<> {
 public:
  explicit RotaryEncoderClockwiseTrigger(RotaryEncoderSensor *parent) {
    parent->add_on_clockwise_callback([this]() { this->trigger(); });
  }
};

class RotaryEncoderAnticlockwiseTrigger : public Trigger<> {
 public:
  explicit RotaryEncoderAnticlockwiseTrigger(RotaryEncoderSensor *parent) {
    parent->add_on_anticlockwise_callback([this]() { this->trigger(); });
  }
};

}  // namespace rotary_encoder
}  // namespace esphome
