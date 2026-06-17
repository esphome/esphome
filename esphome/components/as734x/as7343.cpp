#ifdef USE_AS7343

#include "as7343.h"
#include "esphome/core/log.h"

namespace esphome::as734x {

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
}

bool AS7343::read_channels(uint8_t /*step*/, ChannelValuesUint16 &values, Gain &gain, bool &saturated) {
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

}  // namespace esphome::as734x

#endif  // USE_AS7343
