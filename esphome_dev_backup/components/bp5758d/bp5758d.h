#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"
#include <vector>

namespace esphome {
namespace bp5758d {

class BP5758D : public Component {
 public:
  class Channel;

  void set_data_pin(GPIOPin *data_pin) { data_pin_ = data_pin; }
  void set_clock_pin(GPIOPin *clock_pin) { clock_pin_ = clock_pin; }

  void setup() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Send new values if they were updated.
  void loop() override;

  class Channel : public output::FloatOutput {
   public:
    void set_parent(BP5758D *parent) { parent_ = parent; }
    void set_channel(uint8_t channel) { channel_ = channel; }
    void set_current(uint8_t current) { current_ = current; }

   protected:
    void write_state(float state) override {
      auto amount = static_cast<uint16_t>(state * 0x3FF);
      // We're enforcing channels start at 1 to mach OUT1-OUT5, we must adjust
      // to our 0-based array internally here by subtracting 1.
      this->parent_->set_channel_value_(this->channel_ - 1, amount);
      this->parent_->set_channel_current_(this->channel_ - 1, this->current_);
    }

    BP5758D *parent_;
    uint8_t channel_;
    uint8_t current_;
  };

 protected:
  uint8_t correct_current_level_bits_(uint8_t current);
  void set_channel_value_(uint8_t channel, uint16_t value);
  void set_channel_current_(uint8_t channel, uint8_t current);
  void write_bit_(bool value);
  void write_byte_(uint8_t data);
  void write_buffer_(uint8_t *buffer, uint8_t size);

  GPIOPin *data_pin_;
  GPIOPin *clock_pin_;
  uint8_t update_channel_;
  std::vector<uint8_t> channel_current_;
  std::vector<uint16_t> pwm_amounts_;
  bool update_{true};
};

}  // namespace bp5758d
}  // namespace esphome
