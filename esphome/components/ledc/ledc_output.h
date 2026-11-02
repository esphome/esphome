#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"
#include <cstdint>

#ifdef USE_ESP32

namespace esphome::ledc {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern uint8_t next_ledc_channel;

class LEDCOutput final : public output::FloatOutput, public Component {
 public:
  explicit LEDCOutput(InternalGPIOPin *pin) : pin_(pin) { this->channel_ = next_ledc_channel++; }

  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_frequency(float frequency) { this->frequency_ = frequency; }
  void set_phase_angle(float angle) { this->phase_angle_ = angle; }
#ifdef USE_LEDC_DITHERING
  void set_dithering(bool dithering) { this->dithering_ = dithering; }
#endif
  /// Dynamically change frequency at runtime
  void update_frequency(float frequency) override;

  /// Setup LEDC.
  void setup() override;
  void loop() override;
  void dump_config() override;
  /// HARDWARE setup priority
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Override FloatOutput's write_state.
  void write_state(float state) override;

 protected:
  void write_duty_();

  InternalGPIOPin *pin_;
  uint8_t channel_{};
  uint8_t bit_depth_{};
  bool initialized_ = false;
#ifdef USE_LEDC_DITHERING
  bool dithering_{false};
#endif
  float phase_angle_{0.0f};
  float frequency_{};
  float duty_{0.0f};
  // Duty in sixteenths of a timer tick when dithering is enabled, otherwise whole ticks.
  uint32_t last_duty_{UINT32_MAX};
};

}  // namespace esphome::ledc

#endif
