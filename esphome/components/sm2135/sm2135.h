#pragma once

#include <vector>
#include "esphome/components/output/float_output.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace sm2135 {

enum SM2135Current : uint8_t {
  SM2135_CURRENT_10MA = 0x00,
  SM2135_CURRENT_15MA = 0x01,
  SM2135_CURRENT_20MA = 0x02,
  SM2135_CURRENT_25MA = 0x03,
  SM2135_CURRENT_30MA = 0x04,
  SM2135_CURRENT_35MA = 0x05,
  SM2135_CURRENT_40MA = 0x06,
  SM2135_CURRENT_45MA = 0x07,  // Max value for RGB
  SM2135_CURRENT_50MA = 0x08,
  SM2135_CURRENT_55MA = 0x09,
  SM2135_CURRENT_60MA = 0x0A,
};

class SM2135 : public Component {
 public:
  class Channel;

  void set_data_pin(GPIOPin *data_pin) { this->data_pin_ = data_pin; }
  void set_clock_pin(GPIOPin *clock_pin) { this->clock_pin_ = clock_pin; }

  void set_rgb_current(SM2135Current rgb_current) {
    this->rgb_current_ = rgb_current;
    this->current_mask_ = (this->rgb_current_ << 4) | this->cw_current_;
  }

  void set_cw_current(SM2135Current cw_current) {
    this->cw_current_ = cw_current;
    this->current_mask_ = (this->rgb_current_ << 4) | this->cw_current_;
  }

  void setup() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Send new values if they were updated.
  void loop() override;

  class Channel : public output::FloatOutput {
   public:
    void set_parent(SM2135 *parent) { parent_ = parent; }
    void set_channel(uint8_t channel) { channel_ = channel; }

   protected:
    void write_state(float state) override {
      auto amount = static_cast<uint8_t>(state * 0xff);
      this->parent_->set_channel_value_(this->channel_, amount);
    }

    SM2135 *parent_;
    uint8_t channel_;
  };

 protected:
  void set_channel_value_(uint8_t channel, uint8_t value);
  void sm2135_set_low_(GPIOPin *pin);
  void sm2135_set_high_(GPIOPin *pin);

  void sm2135_start_();
  void sm2135_stop_();
  void write_byte_(uint8_t data);
  void write_buffer_(uint8_t *buffer, uint8_t size);

  GPIOPin *data_pin_;
  GPIOPin *clock_pin_;
  uint8_t current_mask_;
  SM2135Current rgb_current_;
  SM2135Current cw_current_;
  uint8_t update_channel_;
  std::vector<uint8_t> pwm_amounts_;
  bool update_{true};
};

}  // namespace sm2135
}  // namespace esphome
