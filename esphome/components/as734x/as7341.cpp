#include "as7341.h"
#include "esphome/core/log.h"

namespace esphome::as734x {

static const char *const TAG = "as734x.as7341";

static constexpr uint8_t AS7341_ASTATUS = 0x94;
static constexpr uint8_t AS7341_ASTEP = 0xCA;
static constexpr uint8_t AS7341_ATIME = 0x81;
static constexpr uint8_t AS7341_BANK_LOW_MAX = 0x74;
static constexpr uint8_t AS7341_BANK_LOW_MIN = 0x60;
static constexpr uint8_t AS7341_CFG0 = 0xA9;
static constexpr uint8_t AS7341_CFG0_REG_BANK_BIT = 4;
static constexpr uint8_t AS7341_CFG1 = 0xAA;
static constexpr uint8_t AS7341_CFG6 = 0xAF;
static constexpr uint8_t AS7341_CHIP_ID = 0x09;
static constexpr uint8_t AS7341_CONFIG = 0x70;
static constexpr uint8_t AS7341_CONFIG_LED_SEL_BIT = 3;
static constexpr uint8_t AS7341_DATA_0 = 0x95;
static constexpr uint8_t AS7341_ENABLE = 0x80;
static constexpr uint8_t AS7341_ENABLE_PON_BIT = 0;
static constexpr uint8_t AS7341_ENABLE_SMUX_EN_BIT = 4;
static constexpr uint8_t AS7341_ENABLE_SP_EN_BIT = 1;
static constexpr uint8_t AS7341_ID = 0x92;
static constexpr uint8_t AS7341_LED = 0x74;
static constexpr uint8_t AS7341_LED_ACT_BIT = 7;
static constexpr uint8_t AS7341_SMUX_CMD_WRITE = 2;  ///< Write SMUX configuration from RAM to SMUX chain
static constexpr uint8_t AS7341_STATUS2 = 0xA3;
static constexpr uint8_t AS7341_STATUS2_AVALID_BIT = 6;
static constexpr uint8_t AS7341_CFG6_SMUX_CMD_SHIFT = 3;
static constexpr uint8_t AS7341_CONFIG_INIT = 0x00;  // INT_MODE SPM, INT_SEL and LED_SEL cleared
static constexpr uint8_t AS7341_ID_MASK = 0xFC;      // the chip id sits in bits 7:2
static constexpr uint8_t AS7341_ID_SHIFT = 2;

// register map for base class
const RegisterMap AS7341::REG_MAP = {
    .bank_low_min = AS7341_BANK_LOW_MIN,
    .bank_low_max = AS7341_BANK_LOW_MAX,
    .astep = AS7341_ASTEP,
    .atime = AS7341_ATIME,
    .cfg0 = AS7341_CFG0,
    .cfg0_reg_bank_bit = AS7341_CFG0_REG_BANK_BIT,
    .cfg1 = AS7341_CFG1,
    .enable = AS7341_ENABLE,
    .enable_pon_bit = AS7341_ENABLE_PON_BIT,
    .enable_sp_en_bit = AS7341_ENABLE_SP_EN_BIT,
    .enable_smux_en_bit = AS7341_ENABLE_SMUX_EN_BIT,
    .led = AS7341_LED,
    .led_act_bit = AS7341_LED_ACT_BIT,
    .status2 = AS7341_STATUS2,
    .status2_avalid_bit = AS7341_STATUS2_AVALID_BIT,
};

// Datasheet values for the golden device: gain correction per gain step. The last two entries stay
// at zero because this chip has no 1024x or 2048x gain and the schema does not offer them.
const std::array<float, GAIN_COUNT> AS7341::GAIN_CORRECTION = {
    1.0577f, 1.0491f, 1.0479f, 1.0491f, 1.0207f, 1.0158f, 1.0109f, 1.0000f, 1.0003f, 0.9873f, 0.9593f, 0.0f, 0.0f};

float AS7341::get_gain_correction(uint8_t /*channel*/, Gain gain) const {
  return GAIN_CORRECTION[static_cast<uint8_t>(gain)];
}

bool AS7341::enable_led(bool enable) {
  // The LED register only drives the LED while LED_SEL in CONFIG is set (DS000504, register 0x70),
  // so the order matters: hand the pin over before switching on, and switch off before handing it
  // back. Clearing LED_SEL first would leave the LED lit with nothing able to turn it off.
  if (enable) {
    return this->write_register_bit_(AS7341_CONFIG, true, AS7341_CONFIG_LED_SEL_BIT) && AS734xBase::enable_led(true);
  }
  const bool led_off = AS734xBase::enable_led(false);
  const bool released = this->write_register_bit_(AS7341_CONFIG, false, AS7341_CONFIG_LED_SEL_BIT);
  return led_off && released;
}

// Nominal centre wavelengths in band order; the clear channel is wideband, hence WIDEBAND_NM.
const std::array<uint16_t, AS7341::NUM_CHANNELS> AS7341::WAVELENGTHS_NM = {415, 445, 480, 515, 555,
                                                                           590, 630, 680, 910, WIDEBAND_NM};
//  F1   F2   F3   F4   F5   F6   F7   F8   NIR  CLEAR

uint16_t AS7341::get_channel_wavelength(uint8_t channel) const { return WAVELENGTHS_NM[channel]; }
const std::array<ChannelContribution, AS7341::NUM_CHANNELS> AS7341::CONTRIBUTIONS = {{
    {0.005261294f, 5.936230324f, 0.020518273f},
    {0.242615252f, 3.572528495f, 0.01352549f},
    {0.174482552f, 2.914113437f, 0.011585145f},
    {1.479499075f, 2.452851227f, 0.010604986f},
    {1.906818555f, 1.940656433f, 0.008890531f},
    {1.162058142f, 1.533976606f, 0.007441544f},
    {0.462374845f, 1.81983825f, 0.00939359f},
    {-0.017794572f, 1.232942274f, 0.006890812f},
    {-0.260730901f, -1.102368f, -0.004647911f},
    {-0.022604075f, -0.033304813f, -0.000129106f},
}};

const std::array<ChannelTristimulus, AS7341::NUM_CHANNELS> AS7341::TRISTIMULUS = {{
    {0.39814f, 0.01396f, 1.95010f},
    {1.29540f, 0.16748f, 6.45490f},
    {0.36956f, 0.23538f, 2.78010f},
    {0.10902f, 1.42750f, 0.18501f},
    {0.71942f, 1.88670f, 0.15325f},
    {1.78180f, 1.14200f, 0.09539f},
    {1.10110f, 0.46497f, 0.10563f},
    {-0.03991f, -0.02702f, 0.08866f},
    {-0.27597f, -0.24468f, -0.61140f},
    {-0.02347f, -0.01993f, -0.00938f},
}};

ChannelContribution AS7341::get_channel_contribution(uint8_t channel) const { return CONTRIBUTIONS[channel]; }

ChannelTristimulus AS7341::get_channel_tristimulus(uint8_t channel) const { return TRISTIMULUS[channel]; }

AS7341::AS7341(i2c::I2CDevice *i2c_device) : AS734xBase(i2c_device, AS7341::NUM_CHANNELS) {}

bool AS7341::verify_device_id() {
  uint8_t id{0};
  if (!this->read_byte_(AS7341_ID, &id)) {
    ESP_LOGE(TAG, "Could not read chip ID");
    return false;
  }
  ESP_LOGV(TAG, "Read ID: 0x%X", id);
  return ((id & AS7341_ID_MASK) == (AS7341_CHIP_ID << AS7341_ID_SHIFT));
}

bool AS7341::write_default_config() { return this->write_byte_(AS7341_CONFIG, AS7341_CONFIG_INIT); }

bool AS7341::prepare_for_smux_step(uint8_t step) {
  // SMUX Config for F1,F2,F3,F4,NIR,Clear
  static const uint8_t SMUX_CONFIG_STEP0[] = {0x30, 0x01, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x50, 0x00,
                                              0x00, 0x00, 0x20, 0x04, 0x00, 0x30, 0x01, 0x50, 0x00, 0x06};
  // SMUX Config for F5,F6,F7,F8,NIR,Clear
  static const uint8_t SMUX_CONFIG_STEP1[] = {0x00, 0x00, 0x00, 0x40, 0x02, 0x00, 0x10, 0x03, 0x50, 0x10,
                                              0x03, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x50, 0x00, 0x06};

  // Set SMUX command to write
  bool ok = this->write_byte_(AS7341_CFG6, AS7341_SMUX_CMD_WRITE << AS7341_CFG6_SMUX_CMD_SHIFT);

  // Write SMUX configuration based on step
  const uint8_t *config = (step == 0) ? SMUX_CONFIG_STEP0 : SMUX_CONFIG_STEP1;
  for (uint8_t i = 0; i < 20; ++i) {
    ok = this->write_byte_(i, config[i]) && ok;
  }

  // A failed enable leaves SMUX_EN clear, which is what is_smux_busy() reads, so the state machine
  // would take an unconfigured multiplexer for a finished one and publish the previous step's light.
  return this->enable_smux() && ok;
}

bool AS7341::read_channels(uint8_t smux_step, ChannelValuesUint16 &values, bool &saturated) {
  constexpr uint8_t adc_channels = 6;

  // Reading ASTATUS latches the spectral data to that read, and the datasheet ties the guarantee
  // to one consecutive transaction over 0x94 to 0xA0, so status and data are fetched together.
  std::array<uint8_t, 1 + 2 * adc_channels> frame{};
  const bool ret = this->i2c_device_->read_register(AS7341_ASTATUS, frame.data(), frame.size()) == i2c::ERROR_OK;
  if (!ret) {
    ESP_LOGW(TAG, "Could not read spectral data");
    return false;
  }

  const RegAStatus astatus{frame[0]};
  if (astatus.asat_status) {
    ESP_LOGVV(TAG, "AS7341 affected by analog or digital saturation. Readings are not reliable.");
  }

  std::array<uint16_t, adc_channels> raw{};
  this->peak_raw_count_ = 0;
  for (uint8_t i = 0; i < adc_channels; i++) {
    raw[i] = static_cast<uint16_t>(frame[1 + 2 * i] | (frame[2 + 2 * i] << 8));  // low byte first
    if (raw[i] > this->peak_raw_count_) {
      this->peak_raw_count_ = raw[i];
    }
  }

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
    values[8] = raw[5];  // SMUX routes NIR to ADC5
    values[9] = raw[4];  // SMUX routes clear to ADC4
  }
  saturated = astatus.asat_status;  // latched data affected by saturation
  return ret;
}

}  // namespace esphome::as734x
