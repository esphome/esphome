#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::pca9685 {

enum class PhaseBalancer {
  NONE = 0x00,
  LINEAR = 0x01,
};

/// Inverts polarity of channel output signal
inline constexpr uint8_t PCA9685_MODE_INVERTED = 0x10;
/// Channel update happens upon ACK (post-set) rather than on STOP (endTransmission)
inline constexpr uint8_t PCA9685_MODE_OUTPUT_ONACK = 0x08;
/// Use a totem-pole (push-pull) style output rather than an open-drain structure.
inline constexpr uint8_t PCA9685_MODE_OUTPUT_TOTEM_POLE = 0x04;
/// For active low output enable, sets channel output to high-impedance state
inline constexpr uint8_t PCA9685_MODE_OUTNE_HIGHZ = 0x02;
/// Similarly, sets channel output to high if in totem-pole mode, otherwise
inline constexpr uint8_t PCA9685_MODE_OUTNE_LOW = 0x01;

/// PCA9685 float output component.
class PCA9685Output final : public Component, public i2c::I2CDevice {
 public:
  PCA9685Output(uint8_t mode = PCA9685_MODE_OUTPUT_ONACK | PCA9685_MODE_OUTPUT_TOTEM_POLE) : mode_(mode) {}

  void register_channel(uint8_t channel);
  void write_channel(uint8_t channel, float state);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override;
  void set_extclk(bool extclk) { this->extclk_ = extclk; }
  void set_frequency(float frequency) { this->frequency_ = frequency; }
  void set_phase_balancer(PhaseBalancer balancer) { this->balancer_ = balancer; }

 protected:
  /// Configure the chip; false on an I2C failure.
  bool setup_registers_();
  float frequency_;
  uint8_t mode_;
  bool extclk_ = false;
  PhaseBalancer balancer_ = PhaseBalancer::LINEAR;

  uint8_t min_channel_{0xFF};
  uint8_t max_channel_{0x00};
  uint16_t pwm_amounts_[16] = {
      0,
  };
  bool update_{true};
};

}  // namespace esphome::pca9685
