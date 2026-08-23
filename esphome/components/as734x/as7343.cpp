#include "as7343.h"
#include "esphome/core/log.h"

namespace esphome::as734x {

static const char *const TAG = "as734x.as7343";

static constexpr uint8_t AS7343_AGC_GAIN_MAX = 0xD7;
static constexpr uint8_t AS7343_ASTATUS = 0x94;
static constexpr uint8_t AS7343_ASTEP = 0xD4;
static constexpr uint8_t AS7343_ATIME = 0x81;
static constexpr uint8_t AS7343_BANK_LOW_MAX = 0x7F;  // CFG0 bit description, not the 0x58-0x66 overview
static constexpr uint8_t AS7343_BANK_LOW_MIN = 0x20;
static constexpr uint8_t AS7343_CFG0 = 0xBF;
static constexpr uint8_t AS7343_CFG0_REG_BANK_BIT = 4;
static constexpr uint8_t AS7343_CFG1 = 0xC6;
static constexpr uint8_t AS7343_CFG10 = 0x65;
static constexpr uint8_t AS7343_CFG20 = 0xD6;
static constexpr uint8_t AS7343_CFG6 = 0xF5;
static constexpr uint8_t AS7343_CFG8 = 0xC9;
static constexpr uint8_t AS7343_CHAIN_CMD = 0xE4;
static constexpr uint8_t AS7343_CHAIN_SMUX = 0xE7;
static constexpr uint8_t AS7343_CHIP_ID = 0b10000001;
static constexpr uint8_t AS7343_DATA_0 = 0x95;
static constexpr uint8_t AS7343_ENABLE = 0x80;
static constexpr uint8_t AS7343_ENABLE_PON_BIT = 0;
static constexpr uint8_t AS7343_ENABLE_SMUX_EN_BIT = 4;
static constexpr uint8_t AS7343_ENABLE_SP_EN_BIT = 1;
static constexpr uint8_t AS7343_FD_CFG0 = 0xDF;
static constexpr uint8_t AS7343_FD_TIME_1 = 0xE0;
static constexpr uint8_t AS7343_FD_TIME_2 = 0xE2;
static constexpr uint8_t AS7343_ID = 0x5A;
static constexpr uint8_t AS7343_LED = 0xCD;
static constexpr uint8_t AS7343_LED_ACT_BIT = 7;
static constexpr uint8_t AS7343_STATUS = 0x93;
static constexpr uint8_t AS7343_STATUS2 = 0x90;
static constexpr uint8_t AS7343_STATUS2_AVALID_BIT = 6;

static constexpr uint8_t AS7343_CFG0_INIT = 0x00;  // REG_BANK is owned by select_low_bank_()
static constexpr uint8_t AS7343_CFG6_INIT = 0x0;
static constexpr uint8_t AS7343_FD_CFG0_INIT = 0xa1;
static constexpr uint8_t AS7343_CFG10_INIT = 0xf2;
static constexpr uint8_t AS7343_CFG1_INIT = 0x0c;
static constexpr uint8_t AS7343_CFG8_INIT = 0xc8;
static constexpr uint8_t AS7343_AGC_GAIN_MAX_INIT = 0x99;
static constexpr uint8_t AS7343_FD_TIME_1_INIT = 0x64;
static constexpr uint8_t AS7343_FD_TIME_2_INIT = 0x21;
static constexpr uint8_t AS7343_CFG20_AUTO_SMUX_3_CYCLES = 0x62;  // auto_smux 0b11, 18 channels, 3 cycles

const RegisterMap AS7343::REG_MAP = {
    .bank_low_min = AS7343_BANK_LOW_MIN,
    .bank_low_max = AS7343_BANK_LOW_MAX,
    .astep = AS7343_ASTEP,
    .atime = AS7343_ATIME,
    .cfg0 = AS7343_CFG0,
    .cfg0_reg_bank_bit = AS7343_CFG0_REG_BANK_BIT,
    .cfg1 = AS7343_CFG1,
    .enable = AS7343_ENABLE,
    .enable_pon_bit = AS7343_ENABLE_PON_BIT,
    .enable_sp_en_bit = AS7343_ENABLE_SP_EN_BIT,
    .enable_smux_en_bit = AS7343_ENABLE_SMUX_EN_BIT,
    .led = AS7343_LED,
    .led_act_bit = AS7343_LED_ACT_BIT,
    .status2 = AS7343_STATUS2,
    .status2_avalid_bit = AS7343_STATUS2_AVALID_BIT,
};

// Datasheet values for the golden device: gain correction per gain step, per channel.
const std::array<std::array<float, AS7343::NUM_CHANNELS>, GAIN_COUNT> AS7343::GAIN_CORRECTION = {{
    {1.149000f, 1.100000f, 1.060000f, 1.070000f, 1.063000f, 1.051000f, 1.062000f, 1.056000f, 1.049000f, 1.040000f,
     1.080000f, 1.038000f, 1.065000f},  // 0.5x
    {1.090000f, 1.128000f, 1.064000f, 1.071000f, 1.063000f, 1.050000f, 1.068000f, 1.055000f, 1.047000f, 1.039000f,
     1.075000f, 1.038000f, 1.085000f},  // 1x
    {1.083000f, 1.086000f, 1.062000f, 1.070000f, 1.062000f, 1.049000f, 1.057000f, 1.053000f, 1.045000f, 1.038000f,
     1.063000f, 1.037000f, 1.069000f},  // 2x
    {1.059000f, 1.068000f, 1.056000f, 1.066000f, 1.058000f, 1.046000f, 1.051000f, 1.051000f, 1.044000f, 1.036000f,
     1.059000f, 1.035000f, 1.053000f},  // 4x
    {1.100000f, 1.109000f, 1.096000f, 1.108000f, 1.099000f, 1.089000f, 1.091000f, 1.092000f, 1.082000f, 1.078000f,
     1.100000f, 1.076000f, 1.088000f},  // 8x
    {1.099000f, 1.109000f, 1.096000f, 1.108000f, 1.099000f, 1.089000f, 1.091000f, 1.092000f, 1.082000f, 1.078000f,
     1.100000f, 1.075000f, 1.087000f},  // 16x
    {1.088000f, 1.096000f, 1.085000f, 1.097000f, 1.087000f, 1.078000f, 1.079000f, 1.080000f, 1.071000f, 1.067000f,
     1.087000f, 1.064000f, 1.076000f},  // 32x
    {1.083000f, 1.091000f, 1.078000f, 1.090000f, 1.079000f, 1.072000f, 1.072000f, 1.073000f, 1.064000f, 1.062000f,
     1.080000f, 1.057000f, 1.069000f},  // 64x
    {1.076000f, 1.084000f, 1.072000f, 1.085000f, 1.074000f, 1.066000f, 1.062000f, 1.067000f, 1.055000f, 1.056000f,
     1.074000f, 1.051000f, 1.061000f},  // 128x
    {1.067000f, 1.074000f, 1.063000f, 1.075000f, 1.064000f, 1.059000f, 1.055000f, 1.058000f, 1.049000f, 1.051000f,
     1.064000f, 1.044000f, 1.053000f},  // 256x
    {1.000000f, 1.000000f, 1.000000f, 1.000000f, 1.000000f, 1.000000f, 1.000000f, 1.000000f, 1.000000f, 1.000000f,
     1.000000f, 1.000000f, 1.000000f},  // 512x
    {1.033303857f, 1.034763813f, 1.029855013f, 1.036911249f, 0.988150537f, 1.029003501f, 1.021363258f, 0.98762089f,
     1.021713376f, 1.040108204f, 0.987417698f, 1.019481421f, 0.987270057f},  // 1024x
    {0.986859143f, 0.980877221f, 0.9469769f, 0.985535324f, 0.926243961f, 0.959814787f, 1.007963896f, 0.942685187f,
     1.007630587f, 1.025460005f, 0.928696275f, 1.001296639f, 1.041275263f},  // 2048x
}};

float AS7343::get_gain_correction(uint8_t channel, Gain gain) const {
  return GAIN_CORRECTION[static_cast<uint8_t>(gain)][channel];
}

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
  AS7343_CHANNEL_690_F7,
  AS7343_CHANNEL_745_F8,
  AS7343_CHANNEL_550_F5,
  AS7343_CHANNEL_CLEAR,
  AS7343_CHANNEL_FD,

  AS7343_NUM_CHANNELS_MAX
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

// Nominal centre wavelengths in band order; the clear channel is wideband, hence WIDEBAND_NM.
const std::array<uint16_t, AS7343::NUM_CHANNELS> AS7343::WAVELENGTHS_NM = {405, 425, 450, 475, 515, 555,        550,
                                                                           600, 640, 690, 745, 855, WIDEBAND_NM};
//  F1   F2   FZ   F3   F4   FY   F5   FXL  F6   F7   F8   NIR  CLEAR

uint16_t AS7343::get_channel_wavelength(uint8_t channel) const { return WAVELENGTHS_NM[channel]; }
const std::array<ChannelContribution, AS7343::NUM_CHANNELS> AS7343::CONTRIBUTIONS = {{
    {-0.028559089f, 0.450293995f, 0.001266925f},
    {-0.042150948f, 3.566379984f, 0.012515925f},
    {0.000661721f, 2.877424672f, 0.010243005f},
    {-0.108791278f, 2.273837581f, 0.009376157f},
    {-0.16717218f, 1.519298503f, 0.006017556f},
    {5.726660294f, 4.199345859f, 0.019357905f},
    {0.0f, 0.0f, 0.0f},
    {-0.241492014f, 1.18525916f, 0.006516325f},
    {-0.047665296f, 1.759469826f, 0.009780714f},
    {-0.01251699f, 1.702006611f, 0.009950228f},
    {-0.026424974f, -1.059196137f, -0.005425656f},
    {-0.007565221f, -0.064827881f, -0.000232267f},
    {0.0f, 0.0f, 0.0f},
}};

const std::array<ChannelTristimulus, AS7343::NUM_CHANNELS> AS7343::TRISTIMULUS = {{
    {-0.07879f, -0.03269f, -0.31295f},
    {-0.12235f, -0.01297f, -0.57885f},
    {1.99879f, -0.04011f, 10.00197f},
    {-0.33364f, -0.06889f, -0.31281f},
    {-0.06795f, -0.17453f, -0.19657f},
    {-0.45419f, 5.69083f, -0.11077f},
    {0.00000f, 0.00000f, 0.00000f},
    {5.27242f, -0.22956f, -0.06132f},
    {-0.05072f, -0.04762f, -0.03536f},
    {-0.04666f, -0.01295f, -0.06025f},
    {-0.03931f, -0.02797f, -0.12174f},
    {0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f},
}};

ChannelContribution AS7343::get_channel_contribution(uint8_t channel) const { return CONTRIBUTIONS[channel]; }

ChannelTristimulus AS7343::get_channel_tristimulus(uint8_t channel) const { return TRISTIMULUS[channel]; }

AS7343::AS7343(i2c::I2CDevice *i2c_device) : AS734xBase(i2c_device, AS7343::NUM_CHANNELS) {}

bool AS7343::verify_device_id() {
  uint8_t id{0};
  if (!this->read_byte_(AS7343_ID, &id)) {
    ESP_LOGE(TAG, "Could not read chip ID");
    return false;
  }
  ESP_LOGV(TAG, "Read ID: 0x%X", id);
  return (id == AS7343_CHIP_ID);
}

bool AS7343::write_default_config() { return this->direct_config_3_chain_(); }

bool AS7343::direct_config_3_chain_() {
  bool ok = true;
  ok = this->write_byte_(AS7343_CFG6, AS7343_CFG6_INIT) && ok;
  ok = this->write_byte_(AS7343_FD_CFG0, AS7343_FD_CFG0_INIT) && ok;
  ok = this->write_byte_(AS7343_CFG10, AS7343_CFG10_INIT) && ok;
  ok = this->write_byte_(AS7343_CFG0, AS7343_CFG0_INIT) && ok;
  ok = this->write_byte_(AS7343_CFG1, AS7343_CFG1_INIT) && ok;
  ok = this->write_byte_(AS7343_CFG8, AS7343_CFG8_INIT) && ok;
  ok = this->write_byte_(AS7343_CFG20, AS7343_CFG20_AUTO_SMUX_3_CYCLES) && ok;
  ok = this->write_byte_(AS7343_AGC_GAIN_MAX, AS7343_AGC_GAIN_MAX_INIT) && ok;
  ok = this->write_byte_(AS7343_FD_TIME_1, AS7343_FD_TIME_1_INIT) && ok;
  ok = this->write_byte_(AS7343_FD_TIME_2, AS7343_FD_TIME_2_INIT) && ok;

  constexpr uint8_t chains = 3;
  constexpr uint8_t chain_len = 10;

  const uint8_t smux_cmd[chains] = {0x46, 0x56, 0x66};
  const uint8_t adc_map[chains][chain_len] = {{0x00, 0x04, 0x65, 0x02, 0x00, 0x05, 0x00, 0x01, 0x00, 0x30},
                                              {0x00, 0x00, 0x60, 0x20, 0x04, 0x50, 0x03, 0x00, 0x01, 0x05},
                                              {0x05, 0x00, 0x60, 0x00, 0x30, 0x00, 0x40, 0x10, 0x20, 0x00}};
  for (size_t chain = 0; chain < chains; chain++) {
    for (size_t i = 0; i < chain_len; i++) {
      ok = this->write_byte_(AS7343_CHAIN_SMUX, adc_map[chain][i]) && ok;
      ok = this->write_byte_(AS7343_CHAIN_CMD, smux_cmd[chain]) && ok;
    }
  }
  return ok;
}

bool AS7343::read_channels(uint8_t /*step*/, ChannelValuesUint16 &values, bool &saturated) {
  static constexpr uint8_t SMUX_CHANNEL_MAP[NUM_CHANNELS] = {
      AS7343_CHANNEL_405_F1, AS7343_CHANNEL_425_F2, AS7343_CHANNEL_450_FZ, AS7343_CHANNEL_475_F3,
      AS7343_CHANNEL_515_F4, AS7343_CHANNEL_555_FY, AS7343_CHANNEL_550_F5, AS7343_CHANNEL_600_FXL,
      AS7343_CHANNEL_640_F6, AS7343_CHANNEL_690_F7, AS7343_CHANNEL_745_F8, AS7343_CHANNEL_855_NIR,
      AS7343_CHANNEL_CLEAR_0};

  RegStatus status{0};
  this->read_byte_(AS7343_STATUS, &status.raw);
  ESP_LOGVV(TAG, "Status 0x%02x, sint %d, fint %d, aint %d, asat %d", status.raw, status.sint, status.fint, status.aint,
            status.asat);
  this->write_byte_(AS7343_STATUS, status.raw);

  // Reading ASTATUS latches the spectral data to that read, and the datasheet ties the guarantee
  // to one consecutive transaction over 0x94 to 0xB8, so status and data are fetched together.
  std::array<uint8_t, 1 + 2 * AS7343_NUM_CHANNELS_MAX> frame{};
  const bool ret = this->i2c_device_->read_register(AS7343_ASTATUS, frame.data(), frame.size()) == i2c::ERROR_OK;
  if (!ret) {
    ESP_LOGW(TAG, "Could not read spectral data");
    return false;
  }

  const RegAStatus astatus{frame[0]};
  ESP_LOGVV(TAG, "AStatus 0x%02x, again_status %d, asat_status %d", astatus.raw, astatus.again_status,
            astatus.asat_status);

  if (astatus.asat_status) {
    ESP_LOGVV(TAG, "AS7343 affected by analog or digital saturation. Readings are not reliable.");
  }

  std::array<uint16_t, AS7343_NUM_CHANNELS_MAX> data{};
  for (uint8_t i = 0; i < AS7343_NUM_CHANNELS_MAX; i++) {
    data[i] = static_cast<uint16_t>(frame[1 + 2 * i] | (frame[2 + 2 * i] << 8));  // low byte first
  }

  this->peak_raw_count_ = data[AS7343_CHANNEL_CLEAR_1];
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    values[i] = data[SMUX_CHANNEL_MAP[i]];
    if (values[i] > this->peak_raw_count_) {
      this->peak_raw_count_ = values[i];
    }
  }

  ESP_LOGVV(TAG, "Clear channels: cycle1 %u, cycle2 %u, cycle3 %u (unused)", data[AS7343_CHANNEL_CLEAR_1],
            data[AS7343_CHANNEL_CLEAR_0], data[AS7343_CHANNEL_CLEAR]);
  const uint32_t clear_sum =
      static_cast<uint32_t>(data[AS7343_CHANNEL_CLEAR_1]) + data[AS7343_CHANNEL_CLEAR_0];  // cycle 3 always reads 0
  values[NUM_CHANNELS - 1] = static_cast<uint16_t>(clear_sum / 2);

  ESP_LOGV(TAG, "F1;F2;FZ;F3;F4;FY;F5;FXL;F6;F7;F8;NIR;VIS");
  ESP_LOGV(TAG, "%u;%u;%u;%u;%u;%u;%u;%u;%u;%u;%u;%u;%u", values[0], values[1], values[2], values[3], values[4],
           values[5], values[6], values[7], values[8], values[9], values[10], values[11], values[12]);
  saturated = astatus.asat_status;  // latched data affected by saturation
  return ret;
}

}  // namespace esphome::as734x
