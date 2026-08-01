#pragma once
#include "as734x.h"

namespace esphome::as734x {

class AS7343 : public AS734xBase {
 public:
  explicit AS7343(i2c::I2CDevice *i2c_device);

  const RegisterMap &registers() const override { return REG_MAP; }

  bool verify_device_id() override;
  void write_default_config() override;

  uint8_t get_number_of_smux_steps() const override { return NUM_SMUX_STEPS; }
  // AS7343 cycles the SMUX itself, so there is nothing to prepare per step.
  bool prepare_for_smux_step(uint8_t step) override { return true; }
  bool is_smux_busy() override { return false; }

  bool read_channels(uint8_t step, ChannelValuesUint16 &values, Gain &gain, bool &saturated) override;

 protected:
  static constexpr uint8_t NUM_CHANNELS = 13;
  static constexpr uint8_t NUM_SMUX_STEPS = 1;

  static const RegisterMap REG_MAP;

  void direct_config_3_chain_();
};

}  // namespace esphome::as734x
