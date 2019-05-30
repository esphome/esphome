#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/output/float_output.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace ledc {

extern uint8_t next_ledc_channel;

class LEDCOutput : public output::FloatOutput, public Component {
 public:
  explicit LEDCOutput(GPIOPin *pin) : pin_(pin) { this->channel_ = next_ledc_channel++; }

  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_bit_depth(uint8_t bit_depth) { this->bit_depth_ = bit_depth; }
  void set_frequency(float frequency) { this->frequency_ = frequency; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Setup LEDC.
  void setup() override;
  void dump_config() override;
  /// HARDWARE setup priority
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Override FloatOutput's write_state.
  void write_state(float adjusted_value) override;

 protected:
  GPIOPin *pin_;
  uint8_t channel_{};
  uint8_t bit_depth_{};
  float frequency_{};
};

}  // namespace ledc
}  // namespace esphome

#endif
