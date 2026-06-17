#pragma once
#include "as734xbase.h"

namespace esphome::as734x {

class AS7343 : public AS734xBase {
 public:
  AS7343(i2c::I2CDevice *i2c_device);

  const RegisterMap &registers() const override { return REG_MAP; }
  const CalibrationParams &get_default_correction() const override { return DEFAULT_CORRECTION; }

  bool verify_device_id() override;
  void write_default_config() override;

  uint8_t get_number_of_smux_steps() const override { return NUM_SMUX_STEPS; }
  bool prepare_for_smux_step(uint8_t step) override { return true; };
  bool is_smux_ready() override { return true; };

  bool read_and_discard_channels() override;
  bool read_channels(uint8_t step, ChannelValuesUint16 &values, Gain &gain, bool &saturated) override;

  float get_gain_correction(uint8_t channel, Gain gain) override;
  void get_bc_conversion(uint8_t channel, float &mw, float &mw_photopic, float &mw_par, float &umol_ppdf) override;
  void get_xyz_conversion(uint8_t channel, float &tri_x, float &tri_y, float &tri_z) override;

 protected:
  static constexpr uint8_t NUM_CHANNELS = 13;
  static constexpr uint8_t NUM_SMUX_STEPS = 1;

  static const RegisterMap REG_MAP;
  static const CalibrationParams DEFAULT_CORRECTION;
  static const float GAIN_CORRECTION[Gain::MAX_GAIN][NUM_CHANNELS];
  static const float XYZ_PER_COUNT[3][NUM_CHANNELS];
  static const float IRRAD_MW_PER_COUNT[NUM_CHANNELS];
  static const float IRRAD_PAR_E_MW_PER_COUNT[NUM_CHANNELS];
  static const float IRRAD_PHOTOPIC_MW_PER_COUNT[NUM_CHANNELS];
  static const float PPFD_UMOL_PER_COUNT[NUM_CHANNELS];

  void direct_config_3_chain_();
};

}  // namespace esphome::as734x
