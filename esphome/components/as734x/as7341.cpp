#ifdef USE_AS7341

#include "as7341.h"
#include "esphome/core/log.h"

namespace esphome::as734x {

static const char *const TAG = "as734x.as7341";

static constexpr uint8_t AS7341_CHIP_ID = 0x09;

static constexpr uint8_t AS7341_ASTATUS = 0x94;
static constexpr uint8_t AS7341_CFG6 = 0xAF;
static constexpr uint8_t AS7341_CONFIG = 0x70;
static constexpr uint8_t AS7341_DATA_0 = 0x95;
static constexpr uint8_t AS7341_ID = 0x92;
static constexpr uint8_t AS7341_SMUX_CMD_READ = 1;       ///< Read SMUX configuration to RAM from SMUX chain
static constexpr uint8_t AS7341_SMUX_CMD_ROM_RESET = 0;  ///< ROM code initialization of SMUX
static constexpr uint8_t AS7341_SMUX_CMD_WRITE = 2;      ///< Write SMUX configuration from RAM to SMUX chain

// register map for base class
const RegisterMap AS7341::REG_MAP = {
    .ASTEP = 0xCA,
    .ATIME = 0x81,
    .CFG0 = 0xA9,
    .CFG0_REG_BANK_BIT = 4,
    .CFG1 = 0xAA,
    .ENABLE = 0x80,
    .ENABLE_PON_BIT = 0,
    .ENABLE_SP_EN_BIT = 1,
    .ENABLE_SMUX_EN_BIT = 4,
    .LED = 0x74,
    .LED_ACT_BIT = 7,
    .STATUS2 = 0xA3,
    .STATUS2_AVALID_BIT = 6,
};
/////////////////////////////////////////////////////////////////
// 0. Datasheet values from Excel sheet for Golden Device (GD)
// 0.1 Gain correction values for each channel
const float AS7341::GAIN_CORRECTION[Gain::MAX_GAIN] = {1.0577, 1.0491, 1.0479, 1.0491, 1.0207, 1.0158, 1.0109,
                                                       1.0000, 1.0003, 0.9873, 0.9593, 0.0,    0.0};
// 0.2 Default correction values for each channel
const CalibrationParams AS7341::DEFAULT_CORRECTION = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

// 0.3 XYZ caclulation matrix
const float AS7341::XYZ_PER_COUNT[3][NUM_CHANNELS] = {
    {0.39814, 1.29540, 0.36956, 0.10902, 0.71942, 1.78180, 1.10110, -0.03991, -0.27597, -0.02347},
    {0.01396, 0.16748, 0.23538, 1.42750, 1.88670, 1.14200, 0.46497, -0.02702, -0.24468, -0.01993},
    {1.95010, 6.45490, 2.78010, 0.18501, 0.15325, 0.09539, 0.10563, 0.08866, -0.61140, -0.00938}};

// 1. Following matrices are based on Golden Device calibration matrix
//    These are convoluted matrices for quick calculations based on SPD reconstruction
// 1.1. E fullband irradiance (W/m²)
const float AS7341::IRRAD_MW_PER_COUNT[NUM_CHANNELS] = {8.110691277, 0.988166817, 1.513167954, 0.823071218, 0.529502926,
                                                        0.35304952,  0.255581795, 0.132104073, 2.423065737, 0.63983804};
// 1.2. E PAR band 400-700 nm irradiance (W/m²)
const float AS7341::IRRAD_PAR_E_MW_PER_COUNT[NUM_CHANNELS] = {
    5.936230324, 3.572528495, 2.914113437, 2.452851227, 1.940656433,
    1.533976606, 1.81983825,  1.232942274, -1.102368,   -0.033304813,
};

// 1.3 E photopic irradiance (W/m²) - takes into account photopic response curve CIE1931 V(lambda).
// SUM over 380nm-780nm (SPD(lambda)*V(lambda))xdeltaLambda)
const float AS7341::IRRAD_PHOTOPIC_MW_PER_COUNT[NUM_CHANNELS] = {
    0.005261294, 0.242615252, 0.174482552,  1.479499075,  1.906818555,
    1.162058142, 0.462374845, -0.017794572, -0.260730901, -0.022604075,
};

// 1.4 Lux = E photopic irradiance (W/m²) * 683.002 lm/W
// same matrix, multiply the result

// 1.5 Photosyhtetic photon flux density (PPFD) (µmol/s·m²) 400-700 nm only
// K = 0.008359347      # 1e6/(NA·h·c·1e9)  with NA = 6.022 140 76e23, h = 6.626 070 15e-34, c = 2.997 924 58e8
// ppfd = K * integrate(spd[mask] * λ[mask])
const float AS7341::PPFD_UMOL_PER_COUNT[NUM_CHANNELS] = {0.020518273,  0.01352549,  0.011585145, 0.010604986,
                                                         0.008890531,  0.007441544, 0.00939359,  0.006890812,
                                                         -0.004647911, -0.000129106};

AS7341::AS7341(i2c::I2CDevice *i2c_device) : AS734xBase(i2c_device, AS7341::NUM_CHANNELS) {}

bool AS7341::verify_device_id() {
  this->set_bank_for_reg_(AS7341_ID);

  uint8_t id;
  this->i2c_device_->read_byte(AS7341_ID, &id);
  ESP_LOGCONFIG(TAG, "  Read ID: 0x%X", id);
  return ((id & 0xFC) == (AS7341_CHIP_ID << 2));
}

void AS7341::write_default_config() { this->i2c_device_->write_byte(AS7341_CONFIG, 0x00); }

float AS7341::get_gain_correction(uint8_t channel, const Gain gain) {
  return GAIN_CORRECTION[static_cast<uint8_t>(gain)];
};

void AS7341::get_bc_conversion(uint8_t channel, float &mw, float &mw_photopic, float &mw_par, float &umol_ppdf) {
  mw = IRRAD_MW_PER_COUNT[channel];
  mw_photopic = IRRAD_PHOTOPIC_MW_PER_COUNT[channel];
  mw_par = IRRAD_PAR_E_MW_PER_COUNT[channel];
  umol_ppdf = PPFD_UMOL_PER_COUNT[channel];
};

void AS7341::get_xyz_conversion(uint8_t channel, float &tri_x, float &tri_y, float &tri_z) {
  tri_x = XYZ_PER_COUNT[0][channel];
  tri_y = XYZ_PER_COUNT[1][channel];
  tri_z = XYZ_PER_COUNT[2][channel];
}

bool AS7341::prepare_for_smux_step(uint8_t step) {
  // SMUX Config for F1,F2,F3,F4,NIR,Clear
  static const uint8_t smux_config_step0[] = {0x30, 0x01, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x50, 0x00,
                                              0x00, 0x00, 0x20, 0x04, 0x00, 0x30, 0x01, 0x50, 0x00, 0x06};
  // SMUX Config for F5,F6,F7,F8,NIR,Clear
  static const uint8_t smux_config_step1[] = {0x00, 0x00, 0x00, 0x40, 0x02, 0x00, 0x10, 0x03, 0x50, 0x10,
                                              0x03, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x50, 0x00, 0x06};

  // Set SMUX command to write
  this->i2c_device_->write_byte(AS7341_CFG6, AS7341_SMUX_CMD_WRITE << 3);

  // Write SMUX configuration based on step
  const uint8_t *config = (step == 0) ? smux_config_step0 : smux_config_step1;
  for (uint8_t i = 0; i < 20; ++i) {
    this->i2c_device_->write_byte(i, config[i]);
  }
  this->enable_smux();

  return true;
}

bool AS7341::read_and_discard_channels() { return false; }

bool AS7341::read_channels(uint8_t smux_step, ChannelValuesUint16 &values, Gain &gain, bool &saturated) {
  constexpr uint8_t ADC_CHANNELS = 6;

  std::array<uint16_t, ADC_CHANNELS> raw{};
  RegAStatus astatus{};
  this->i2c_device_->read_byte(AS7341_ASTATUS, &astatus.raw);
  if (astatus.asat_status) {
    ESP_LOGVV(TAG, "AS7341 affected by analog or digital saturation. Readings are not reliable.");
  }

  bool ret = this->i2c_device_->read_bytes_16(AS7341_DATA_0, raw.data(), ADC_CHANNELS);
  if (smux_step == 0) {
    values[0] = raw[0];
    values[1] = raw[1];
    values[2] = raw[2];
    values[3] = raw[3];
  } else if (smux_step == 1) {
    values[4] = raw[0];
    values[5] = raw[1];
    values[6] = raw[2];
    values[7] = raw[3];
    values[8] = raw[4];
    values[9] = raw[5];
  }
  gain = astatus.again_status;      // gain applied to the latest spectral measurement
  saturated = astatus.asat_status;  // latched data affected by saturation
  return ret;
}

}  // namespace esphome::as734x

#endif  // USE_AS7341
