#pragma once
// TLC5947 24-Channel, 12-Bit PWM LED Driver
// https://www.ti.com/lit/ds/symlink/tlc5947.pdf

#include <vector>
#include "esphome/components/output/float_output.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace tlc5947 {

class TLC5947 : public Component {
 public:
  const uint8_t N_CHANNELS_PER_CHIP = 24;

  void set_data_pin(GPIOPin *data_pin) { data_pin_ = data_pin; }
  void set_clock_pin(GPIOPin *clock_pin) { clock_pin_ = clock_pin; }
  void set_lat_pin(GPIOPin *lat_pin) { lat_pin_ = lat_pin; }
  void set_outenable_pin(GPIOPin *outenable_pin) { outenable_pin_ = outenable_pin; }
  void set_num_chips(uint8_t num_chips) { num_chips_ = num_chips; }

  void setup() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Send new values if they were updated.
  void loop() override;

  void set_channel_value(uint16_t channel, uint16_t value);

 protected:
  GPIOPin *data_pin_;
  GPIOPin *clock_pin_;
  GPIOPin *lat_pin_;
  GPIOPin *outenable_pin_{nullptr};
  uint8_t num_chips_;

  std::vector<uint16_t> pwm_amounts_;
  bool update_{true};
};

}  // namespace tlc5947
}  // namespace esphome
