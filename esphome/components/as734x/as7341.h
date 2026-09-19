#pragma once
#include "as734x.h"

namespace esphome::as734x {

class AS7341 : public AS734xBase {
 public:
  explicit AS7341(i2c::I2CDevice *i2c_device);

  bool verify_device_id() override;
  bool write_default_config() override;

  bool enable_led(bool enable) override;
  float get_gain_correction(uint8_t channel, Gain gain) const override;
  uint16_t get_channel_wavelength(uint8_t channel) const override;

  uint8_t get_number_of_smux_steps() const override { return NUM_SMUX_STEPS; }
  uint8_t get_integration_cycles() const override { return 1; }
  bool prepare_for_smux_step(uint8_t step) override;

  bool read_channels(uint8_t step, ChannelValuesUint16 &values, bool &saturated) override;

 protected:
  const RegisterMap &registers() const override { return REG_MAP; }

  static constexpr uint8_t NUM_CHANNELS = 10;
  static constexpr uint8_t NUM_SMUX_STEPS = 2;

  static const RegisterMap REG_MAP;
  static const std::array<uint16_t, NUM_CHANNELS> WAVELENGTHS_NM;
  // Gain correction is the same for every channel on this chip.
  static const std::array<float, GAIN_COUNT> GAIN_CORRECTION;
};

}  // namespace esphome::as734x
