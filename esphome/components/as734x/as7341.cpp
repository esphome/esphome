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
static constexpr uint8_t AS7341_SMUX_CMD_WRITE = 2;  ///< Write SMUX configuration from RAM to SMUX chain

// register map for base class
const RegisterMap AS7341::REG_MAP = {
    .BANK_LOW_MIN = 0x60,
    .BANK_LOW_MAX = 0x74,
    .ASTEP = 0xCA,
    .ATIME = 0x81,
    .CFG0 = 0xA9,
    .CFG0_REG_BANK_BIT = 4,
    .CFG1 = 0xAA,
    .ENABLE = 0x80,
    .ENABLE_PON_BIT = 0,
    .ENABLE_SP_EN_BIT = 1,
    .ENABLE_SMUX_EN_BIT = 4,
    .STATUS2 = 0xA3,
    .STATUS2_AVALID_BIT = 6,
};

AS7341::AS7341(i2c::I2CDevice *i2c_device) : AS734xBase(i2c_device, AS7341::NUM_CHANNELS) {}

bool AS7341::verify_device_id() {
  this->set_bank_for_reg_(AS7341_ID);

  uint8_t id{0};
  this->i2c_device_->read_byte(AS7341_ID, &id);
  ESP_LOGCONFIG(TAG, "  Read ID: 0x%X", id);
  return ((id & 0xFC) == (AS7341_CHIP_ID << 2));
}

void AS7341::write_default_config() {
  this->set_bank_for_reg_(AS7341_CONFIG);  // CONFIG sits in the low register bank
  this->i2c_device_->write_byte(AS7341_CONFIG, 0x00);
}

bool AS7341::prepare_for_smux_step(uint8_t step) {
  // SMUX Config for F1,F2,F3,F4,NIR,Clear
  static const uint8_t SMUX_CONFIG_STEP0[] = {0x30, 0x01, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x50, 0x00,
                                              0x00, 0x00, 0x20, 0x04, 0x00, 0x30, 0x01, 0x50, 0x00, 0x06};
  // SMUX Config for F5,F6,F7,F8,NIR,Clear
  static const uint8_t SMUX_CONFIG_STEP1[] = {0x00, 0x00, 0x00, 0x40, 0x02, 0x00, 0x10, 0x03, 0x50, 0x10,
                                              0x03, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x50, 0x00, 0x06};

  // Set SMUX command to write
  this->i2c_device_->write_byte(AS7341_CFG6, AS7341_SMUX_CMD_WRITE << 3);

  // Write SMUX configuration based on step
  const uint8_t *config = (step == 0) ? SMUX_CONFIG_STEP0 : SMUX_CONFIG_STEP1;
  for (uint8_t i = 0; i < 20; ++i) {
    this->i2c_device_->write_byte(i, config[i]);
  }
  this->enable_smux();

  return true;
}

bool AS7341::read_channels(uint8_t smux_step, ChannelValuesUint16 &values, Gain &gain, bool &saturated) {
  constexpr uint8_t ADC_CHANNELS = 6;

  std::array<uint16_t, ADC_CHANNELS> raw{};
  RegAStatus astatus{};
  this->i2c_device_->read_byte(AS7341_ASTATUS, &astatus.raw);
  if (astatus.asat_status) {
    ESP_LOGVV(TAG, "AS7341 affected by analog or digital saturation. Readings are not reliable.");
  }

  // The data registers hold the low byte first, but read_bytes_16() converts from big endian,
  // so every word needs swapping back.
  bool ret = this->i2c_device_->read_bytes_16(AS7341_DATA_0, raw.data(), ADC_CHANNELS);
  for (auto &value : raw) {
    value = this->swap_bytes_(value);
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
    values[8] = raw[4];
    values[9] = raw[5];
  }
  gain = astatus.again_status;      // gain applied to the latest spectral measurement
  saturated = astatus.asat_status;  // latched data affected by saturation
  return ret;
}

}  // namespace esphome::as734x

#endif  // USE_AS7341
