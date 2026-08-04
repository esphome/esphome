#pragma once
#include "as734x.h"

namespace esphome::as734x {

class AS7343 : public AS734xBase {
 public:
  explicit AS7343(i2c::I2CDevice *i2c_device);

  bool verify_device_id() override;
  bool write_default_config() override;

  uint8_t get_number_of_smux_steps() const override { return NUM_SMUX_STEPS; }
  uint8_t get_integration_cycles() const override { return 3; }       // auto_smux runs three cycles per frame
  bool prepare_for_smux_step(uint8_t step) override { return true; }  // the AS7343 cycles the SMUX itself
  bool is_smux_busy() override { return false; }

  bool read_channels(uint8_t step, ChannelValuesUint16 &values, Gain &gain, bool &saturated) override;

 protected:
  const RegisterMap &registers() const override { return REG_MAP; }

  static constexpr uint8_t NUM_CHANNELS = 13;
  static constexpr uint8_t NUM_SMUX_STEPS = 1;

  static const RegisterMap REG_MAP;

  bool direct_config_3_chain_();
};

}  // namespace esphome::as734x
