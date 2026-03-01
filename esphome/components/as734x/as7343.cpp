#ifdef USE_AS7343

#include "as7343.h"

namespace esphome {
namespace as734x {

static const char *const TAG = "as734x.as7343";

static constexpr uint8_t AS7343_CHIP_ID = 0b10000001;

static constexpr uint8_t AS7343_AGC_GAIN_MAX = 0xD7;
static constexpr uint8_t AS7343_ASTATUS = 0x94;
static constexpr uint8_t AS7343_CFG0 = 0xBF;
static constexpr uint8_t AS7343_CFG1 = 0xC6;
static constexpr uint8_t AS7343_CFG10 = 0x65;
static constexpr uint8_t AS7343_CFG20 = 0xD6;
static constexpr uint8_t AS7343_CFG6 = 0xF5;
static constexpr uint8_t AS7343_CFG8 = 0xC9;
static constexpr uint8_t AS7343_CHAIN_CMD = 0xE4;
static constexpr uint8_t AS7343_CHAIN_SMUX = 0xE7;
static constexpr uint8_t AS7343_DATA_0 = 0x95;
static constexpr uint8_t AS7343_FD_CFG0 = 0xDF;
static constexpr uint8_t AS7343_FD_TIME_1 = 0xE0;
static constexpr uint8_t AS7343_FD_TIME_2 = 0xE2;
static constexpr uint8_t AS7343_ID = 0x5A;
static constexpr uint8_t AS7343_STATUS = 0x93;

const RegisterMap AS7343::REG_MAP = {
    .ASTEP = 0xD4,
    .ATIME = 0x81,
    .CFG0 = AS7343_CFG0,
    .CFG0_REG_BANK_BIT = 4,
    .CFG1 = AS7343_CFG1,
    .ENABLE = 0x80,
    .ENABLE_PON_BIT = 0,
    .ENABLE_SP_EN_BIT = 1,
    .ENABLE_SMUX_EN_BIT = 4,
    .LED = 0xCD,
    .LED_ACT_BIT = 7,
    .STATUS2 = 0x90,
    .STATUS2_AVALID_BIT = 6,
};

enum AS7343Channel : uint8_t {
  AS7343_CHANNEL_450_FZ,
  AS7343_CHANNEL_555_FY,
  AS7343_CHANNEL_600_FXL,
  AS7343_CHANNEL_855_NIR,
  AS7343_CHANNEL_CLEAR_1,
  AS7343_CHANNEL_FD_1,

  AS7343_CHANNEL_425_F2,
  AS7343_CHANNEL_475_F3,
  AS7343_CHANNEL_515_F4,
  AS7343_CHANNEL_640_F6,
  AS7343_CHANNEL_CLEAR_0,
  AS7343_CHANNEL_FD_0,

  AS7343_CHANNEL_405_F1,
  AS7343_CHANNEL_550_F5,
  AS7343_CHANNEL_690_F7,
  AS7343_CHANNEL_745_F8,
  AS7343_CHANNEL_CLEAR,
  AS7343_CHANNEL_FD,

  AS7343_NUM_CHANNELS_MAX
};

union RegCfg20 {
  uint8_t raw;
  struct {
    uint8_t reserved : 5;
    uint8_t auto_smux : 2;
    uint8_t fd_fifo_8b : 1;
  } __attribute__((packed));
};

union RegStatus {
  uint8_t raw;
  struct {
    uint8_t sint : 1;
    uint8_t reserved_1 : 1;
    uint8_t fint : 1;
    uint8_t aint : 1;
    uint8_t reserved_4_6 : 3;
    uint8_t asat : 1;
  } __attribute__((packed));
};

// clang-format off
/////////////////////////////////////////////////////////////////
// 0. Datasheet values from Excel sheet
// 0.1 Gain correction values for each channel
const float AS7343::GAIN_CORRECTION[Gain::MAX_GAIN][NUM_CHANNELS]  = {
    {1.149000, 1.100000, 1.060000, 1.070000, 1.063000, 1.051000, 1.062000, 1.056000, 1.049000, 1.040000, 1.080000, 1.038000, 1.065000},  // 0.5x
    {1.090000, 1.128000, 1.064000, 1.071000, 1.063000, 1.050000, 1.068000, 1.055000, 1.047000, 1.039000, 1.075000, 1.038000, 1.085000},  // 1x
    {1.083000, 1.086000, 1.062000, 1.070000, 1.062000, 1.049000, 1.057000, 1.053000, 1.045000, 1.038000, 1.063000, 1.037000, 1.069000},  // 2x
    {1.059000, 1.068000, 1.056000, 1.066000, 1.058000, 1.046000, 1.051000, 1.051000, 1.044000, 1.036000, 1.059000, 1.035000, 1.053000},  // 4x
    {1.100000, 1.109000, 1.096000, 1.108000, 1.099000, 1.089000, 1.091000, 1.092000, 1.082000, 1.078000, 1.100000, 1.076000, 1.088000},  // 8x
    {1.099000, 1.109000, 1.096000, 1.108000, 1.099000, 1.089000, 1.091000, 1.092000, 1.082000, 1.078000, 1.100000, 1.075000, 1.087000},  // 16x
    {1.088000, 1.096000, 1.085000, 1.097000, 1.087000, 1.078000, 1.079000, 1.080000, 1.071000, 1.067000, 1.087000, 1.064000, 1.076000},  // 32x
    {1.083000, 1.091000, 1.078000, 1.090000, 1.079000, 1.072000, 1.072000, 1.073000, 1.064000, 1.062000, 1.080000, 1.057000, 1.069000},  // 64x
    {1.076000, 1.084000, 1.072000, 1.085000, 1.074000, 1.066000, 1.062000, 1.067000, 1.055000, 1.056000, 1.074000, 1.051000, 1.061000},  // 128x
    {1.067000, 1.074000, 1.063000, 1.075000, 1.064000, 1.059000, 1.055000, 1.058000, 1.049000, 1.051000, 1.064000, 1.044000, 1.053000},  // 256x
    {1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000},  // 512x
    {1.033303857, 1.034763813, 1.029855013, 1.036911249, 0.988150537, 1.029003501, 1.021363258, 0.98762089, 1.021713376, 1.040108204, 0.987417698, 1.019481421, 0.987270057},  // 1024x
    {0.986859143, 0.980877221, 0.9469769, 0.985535324, 0.926243961, 0.959814787, 1.007963896, 0.942685187, 1.007630587, 1.025460005, 0.928696275, 1.001296639, 1.041275263},  // 2048x
};

// 0.2 Default GD correction for each channel
const CalibrationParams AS7343::DEFAULT_CORRECTION = {
    1.055464349, 1.043509797, 1.029576268, 1.0175052,   1.00441899,  0.987356499, 0.957597044,
    0.995863485, 1.014628964, 0.996500814, 0.933072749, 1.052236338, 0.999570232
};

// 0.3 GD XYZ calculation Matrix per Channel (copy from calculation CM), channels F5, NIR and CLEAR are not used = 0
const float AS7343::XYZ_PER_COUNT[3][NUM_CHANNELS] = {
    {-0.07879, -0.12235, 1.99879, -0.33364, -0.06795, -0.45419, 0.00000, 5.27242, -0.05072, -0.04666, -0.03931, 0, 0},
    {-0.03269, -0.01297, -0.04011, -0.06889, -0.17453, 5.69083, 0.00000, -0.22956, -0.04762, -0.01295, -0.02797, 0, 0},
    {-0.31295, -0.57885, 10.00197, -0.31281, -0.19657, -0.11077, 0.00000, -0.06132, -0.03536, -0.06025, -0.12174, 0, 0}
};

// 1. Following matrices are based on Golden Device calibration matrix
//    These are convoluted matrices for quick calculations based on SPD reconstruction
// 1.1. E fullband irradiance (W/m²)
const float AS7343::IRRAD_MW_PER_COUNT[NUM_CHANNELS] = {
    15.24248081,3.761606341,-5.578085497,8.400505925,2.780903659,0.850080092,0,4.126948838,0.663050905,2.146279738,3.908525219,6.820076603,0
};

// 1.2. E PAR band 400-700 nm irradiance (W/m²)
const float AS7343::IRRAD_PAR_E_MW_PER_COUNT[NUM_CHANNELS] = {
    0.450293995,3.566379984,2.877424672,2.273837581,1.519298503,4.199345859,0,1.18525916,1.759469826,1.702006611,-1.059196137,-0.064827881,0
};

// 1.3 E photopic irradiance (W/m²) - takes into account photopic response curve CIE1931 V(lambda).
// SUM over 380nm-780nm (SPD(lambda)*V(lambda))xdeltaLambda)
const float AS7343::IRRAD_PHOTOPIC_MW_PER_COUNT[NUM_CHANNELS] = {
    -0.028559089,-0.042150948,0.000661721,-0.108791278,-0.16717218,5.726660294,0,-0.241492014,-0.047665296,-0.01251699,-0.026424974,-0.007565221,0
};

// 1.4 Lux = E photopic irradiance (W/m²) * 683.002 lm/W
// no need for the matrix - its the same, multiply the result

// 1.5 Photosyhtetic photon flux density (PPFD) (µmol/s·m²) 400-700 nm only
// K = 0.008359347      # 1e6/(NA·h·c·1e9)  with NA = 6.022 140 76e23, h = 6.626 070 15e-34, c = 2.997 924 58e8
// ppfd = K * integrate(spd[mask] * λ[mask])
const float AS7343::PPFD_UMOL_PER_COUNT[NUM_CHANNELS] = {
    0.001266925,0.012515925,0.010243005,0.009376157,0.006017556,0.019357905,0,0.006516325,0.009780714,0.009950228,-0.005425656,-0.000232267,0
};
// clang-format on

AS7343::AS7343(i2c::I2CDevice *i2c_device) : AS734xBase(i2c_device, AS7343::NUM_CHANNELS) {}

bool AS7343::verify_device_id() {
  this->set_bank_for_reg_(AS7343_ID);

  uint8_t id;
  this->i2c_device_->read_byte(AS7343_ID, &id);
  ESP_LOGCONFIG(TAG, "  Read ID: 0x%X", id);
  return (id == AS7343_CHIP_ID);
}

void AS7343::write_default_config() {
  // Set configuration
  RegCfg20 cfg20;
  cfg20.raw = this->i2c_device_->reg(AS7343_CFG20).get();
  cfg20.auto_smux = 0b11;
  this->i2c_device_->reg(AS7343_CFG20) = cfg20.raw;

  this->direct_config_3_chain_();
}

void AS7343::direct_config_3_chain_() {
  this->i2c_device_->write_byte(AS7343_CFG6, 0x0);
  this->i2c_device_->write_byte(AS7343_FD_CFG0, 0xa1);
  this->i2c_device_->write_byte(AS7343_CFG10, 0xf2);

  this->i2c_device_->write_byte(AS7343_CFG0, 0x10);
  this->i2c_device_->write_byte(AS7343_CFG1, 0x0c);
  this->i2c_device_->write_byte(AS7343_CFG8, 0xc8);
  this->i2c_device_->write_byte(AS7343_CFG20, 0x62);
  this->i2c_device_->write_byte(AS7343_AGC_GAIN_MAX, 0x99);
  this->i2c_device_->write_byte(AS7343_FD_TIME_1, 0x64);
  this->i2c_device_->write_byte(AS7343_FD_TIME_2, 0x21);

  constexpr uint8_t CHAINS = 3;
  constexpr uint8_t CHAIN_LEN = 10;

  const uint8_t SMUX_CMD[CHAINS] = {0x46, 0x56, 0x66};
  const uint8_t ADC_MAP[CHAINS][CHAIN_LEN] = {{0x00, 0x04, 0x65, 0x02, 0x00, 0x05, 0x00, 0x01, 0x00, 0x30},
                                              {0x00, 0x00, 0x60, 0x20, 0x04, 0x50, 0x03, 0x00, 0x01, 0x05},
                                              {0x05, 0x00, 0x60, 0x00, 0x30, 0x00, 0x40, 0x10, 0x20, 0x00}};
  for (size_t chain = 0; chain < CHAINS; chain++) {
    for (size_t i = 0; i < CHAIN_LEN; i++) {
      this->i2c_device_->write_byte(AS7343_CHAIN_SMUX, ADC_MAP[chain][i]);
      this->i2c_device_->write_byte(AS7343_CHAIN_CMD, SMUX_CMD[chain]);
    }
  }

  // this->write_byte((uint8_t)0x80, 0x11);
}

float AS7343::get_gain_correction(uint8_t channel, Gain gain) {
  return GAIN_CORRECTION[static_cast<uint8_t>(gain)][channel];
};

void AS7343::get_bc_conversion(uint8_t channel, float &mw, float &mw_photopic, float &mw_par, float &umol_ppdf) {
  mw = IRRAD_MW_PER_COUNT[channel];
  mw_photopic = IRRAD_PHOTOPIC_MW_PER_COUNT[channel];
  mw_par = IRRAD_PAR_E_MW_PER_COUNT[channel];
  umol_ppdf = PPFD_UMOL_PER_COUNT[channel];
};

void AS7343::get_xyz_conversion(uint8_t channel, float &tri_x, float &tri_y, float &tri_z) {
  tri_x = XYZ_PER_COUNT[0][channel];
  tri_y = XYZ_PER_COUNT[1][channel];
  tri_z = XYZ_PER_COUNT[2][channel];
}

bool AS7343::read_and_discard_channels() {
  std::array<uint16_t, MAX_CHANNELS> data;
  return this->i2c_device_->read_bytes_16(AS7343_DATA_0, data.data(), NUM_CHANNELS);
}

bool AS7343::read_channels(uint8_t step, ChannelValuesUint16 &values, Gain &gain, bool &saturated) {
  static constexpr uint8_t SMUX_CHANNEL_MAP[NUM_CHANNELS] = {
      AS7343_CHANNEL_405_F1, AS7343_CHANNEL_425_F2, AS7343_CHANNEL_450_FZ, AS7343_CHANNEL_475_F3,
      AS7343_CHANNEL_515_F4, AS7343_CHANNEL_555_FY, AS7343_CHANNEL_550_F5, AS7343_CHANNEL_600_FXL,
      AS7343_CHANNEL_640_F6, AS7343_CHANNEL_690_F7, AS7343_CHANNEL_745_F8, AS7343_CHANNEL_855_NIR,
      AS7343_CHANNEL_CLEAR_0};

  std::array<uint16_t, AS7343_NUM_CHANNELS_MAX> data;

  RegStatus status{0};
  status.raw = this->i2c_device_->reg(AS7343_STATUS).get();
  ESP_LOGVV(TAG, "Status 0x%02x, sint %d, fint %d, aint %d, asat %d", status.raw, status.sint, status.fint, status.aint,
            status.asat);
  this->i2c_device_->reg(AS7343_STATUS) = status.raw;

  RegAStatus astatus{0};
  astatus.raw = this->i2c_device_->reg(AS7343_ASTATUS).get();
  ESP_LOGVV(TAG, "AStatus 0x%02x, again_status %d, asat_status %d", astatus.raw, astatus.again_status,
            astatus.asat_status);

  if (astatus.asat_status) {
    ESP_LOGVV(TAG, "AS7343 affected by analog or digital saturation. Readings are not reliable.");
  }

  auto ret = this->i2c_device_->read_bytes_16(AS7343_DATA_0, data.data(), AS7343_NUM_CHANNELS_MAX);

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    values[i] = data[SMUX_CHANNEL_MAP[i]];
  }

  // combine two clear channels to one
  uint16_t clear = data[AS7343_CHANNEL_CLEAR_0] / 2 + data[AS7343_CHANNEL_CLEAR_1] / 2;
  values[NUM_CHANNELS - 1] = clear;

  gain = astatus.again_status;      // gain applied to the latest spectral measurement
  saturated = astatus.asat_status;  // latched data affected by saturation
  return ret;
}

}  // namespace as734x
}  // namespace esphome

#endif  // USE_AS7343
