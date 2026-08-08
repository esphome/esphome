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
    .status2 = AS7343_STATUS2,
    .status2_avalid_bit = AS7343_STATUS2_AVALID_BIT,
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

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    values[i] = data[SMUX_CHANNEL_MAP[i]];
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
