#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"
#include <vector>

namespace esphome {
namespace my9231 {

/// MY9231 float output component.
class MY9231OutputComponent : public Component {
 public:
  class Channel;
  void set_pin_di(GPIOPin *pin_di) { pin_di_ = pin_di; }
  void set_pin_dcki(GPIOPin *pin_dcki) { pin_dcki_ = pin_dcki; }

  void set_num_channels(uint16_t num_channels) { this->num_channels_ = num_channels; }
  void set_num_chips(uint8_t num_chips) { this->num_chips_ = num_chips; }
  void set_bit_depth(uint8_t bit_depth) { this->bit_depth_ = bit_depth; }

  /// Setup the MY9231.
  void setup() override;
  void dump_config() override;
  /// HARDWARE setup_priority
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  /// Send new values if they were updated.
  void loop() override;

  class Channel : public output::FloatOutput {
   public:
    void set_parent(MY9231OutputComponent *parent) { parent_ = parent; }
    void set_channel(uint8_t channel) { channel_ = channel; }

   protected:
    void write_state(float state) override {
      auto amount = uint16_t(state * this->parent_->get_max_amount_());
      this->parent_->set_channel_value_(this->channel_, amount);
    }

    MY9231OutputComponent *parent_;
    uint8_t channel_;
  };

 protected:
  uint16_t get_max_amount_() const { return (uint32_t(1) << this->bit_depth_) - 1; }

  void set_channel_value_(uint8_t channel, uint16_t value);
  void init_chips_(uint8_t command);
  void write_word_(uint16_t value, uint8_t bits);
  void send_di_pulses_(uint8_t count);
  void send_dcki_pulses_(uint8_t count);

  GPIOPin *pin_di_;
  GPIOPin *pin_dcki_;
  uint8_t bit_depth_;
  uint16_t num_channels_;
  uint8_t num_chips_;
  std::vector<uint16_t> pwm_amounts_;
  bool update_{true};
};

}  // namespace my9231
}  // namespace esphome
