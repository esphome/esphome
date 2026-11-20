#include "at581x.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <iterator>

namespace esphome::at581x {

ESPHOME_LOG_TAG(TAG, "at581x");

/*!< Frequency points in MHz (p14/15 of datasheet). */
static constexpr uint16_t FREQ_TABLE[] PROGMEM = {5696, 5715, 5730, 5748, 5765, 5784,
                                                  5800, 5819, 5836, 5851, 5869, 5888};
static constexpr uint8_t FREQ_MODE_ADDR = 0x61;
static constexpr uint8_t FREQ_MODE_VALUE = 0xCA;  // 0xC0 | 0x02 (freq present) | 0x08 (gain present)
static constexpr uint8_t FREQ_LO_ADDR = 0x5F;
static constexpr uint8_t FREQ_HI_ADDR = 0x60;

/*!< Power consumption points in µA (p12). */
static constexpr uint8_t POWER_TABLE[] PROGMEM = {48, 56, 63, 70, 77, 91, 105, 115, 40, 44, 47, 51, 54, 61, 68, 78};
static constexpr uint8_t POWER_LO_ADDR = 0x67;
static constexpr uint8_t POWER_HI_ADDR = 0x68;
static constexpr uint8_t PWR_WORK_TIME_EN = 8;     // Reg 0x67
static constexpr uint8_t PWR_BURST_TIME_EN = 32;   // Reg 0x68
static constexpr uint8_t PWR_THRESH_EN = 64;       // Reg 0x68
static constexpr uint8_t PWR_THRESH_VAL_EN = 128;  // Reg 0x67

/* Gain for AT581X (3dB per step for level1, 6dB per step for level 2), high value = small gain. (p12) */
static constexpr uint8_t GAIN_LEVEL1_ADDR = 0x5C;
static constexpr uint8_t GAIN_LEVEL2_ADDR = 0x63;
static constexpr uint8_t MAX_GAIN = 12;

static constexpr uint8_t SIGNAL_DETECTION_THRESHOLD_ADDR = 0x10;  // 2 bytes, up to 0x11
static constexpr uint8_t SELF_CHECK_TIME_ADDR = 0x38;             // 2 bytes, up to 0x39
static constexpr uint8_t TRIGGER_BASE_TIME_ADDR = 0x3D;           // 4 bytes, up to 0x40
static constexpr uint8_t TIME_ENABLE_ADDR = 0x41;
static constexpr uint8_t TRIGGER_KEEP_TIME_ADDR = 0x42;  // 4 bytes, up to 0x45
static constexpr uint8_t PROTECT_TIME_ADDR = 0x4E;       // 2 bytes, up to 0x4F
// Don't know why it's required in other code, it's not in datasheet
static constexpr uint8_t ENABLE_ADDR = 0x55;
static constexpr uint8_t RESET_ADDR = 0x00;

/*!< Values for RF and analog modules switch (p10). */
static constexpr i2c::I2CRegValue RF_ON[] PROGMEM = {{0x5d, 0x45}, {0x62, 0x55}, {0x51, 0xA0}};
static constexpr i2c::I2CRegValue RF_OFF[] PROGMEM = {{0x5d, 0x46}, {0x62, 0xaa}, {0x51, 0x50}};
static constexpr i2c::I2CRegValue RESET_SEQUENCE[] PROGMEM = {{RESET_ADDR, 0}, {RESET_ADDR, 1}};

void AT581XComponent::setup() {
#ifdef USE_SWITCH
  if (this->rf_power_switch_ != nullptr)
    this->rf_power_switch_->restore(true);
#endif
}

void AT581XComponent::dump_config() { LOG_I2C_DEVICE(this); }

void AT581XComponent::set_frequency(int frequency) {
  this->freq_index_ = 0;
  while (this->freq_index_ < std::size(FREQ_TABLE) && progmem_read_uint16(&FREQ_TABLE[this->freq_index_]) != frequency)
    this->freq_index_++;
}

void AT581XComponent::set_power_consumption(int value) {
  this->power_index_ = 0;
  while (this->power_index_ < std::size(POWER_TABLE) && progmem_read_byte(&POWER_TABLE[this->power_index_]) != value)
    this->power_index_++;
}

bool AT581XComponent::write_le_(uint8_t addr, uint32_t value, uint8_t size) {
  for (uint8_t i = 0; i < size; i++) {
    if (!this->write_byte(addr + i, static_cast<uint8_t>(value >> (8 * i))))
      return false;
  }
  return true;
}

bool AT581XComponent::i2c_write_config() {
  if (this->freq_index_ >= std::size(FREQ_TABLE) || this->power_index_ >= std::size(POWER_TABLE) ||
      this->gain_ > MAX_GAIN) {
    ESP_LOGE(TAG, "Unsupported frequency, power consumption or gain");
    return false;
  }
  ESP_LOGD(TAG,
           "Writing config\n"
           "  Frequency: %uMHz\n"
           "  Sensing distance: %u\n"
           "  Power: %uµA\n"
           "  Gain: %u\n"
           "  Trigger base time: %ums\n"
           "  Trigger keep time: %ums\n"
           "  Protect time: %ums\n"
           "  Self check time: %ums",
           progmem_read_uint16(&FREQ_TABLE[this->freq_index_]), 1023 - this->delta_,
           progmem_read_byte(&POWER_TABLE[this->power_index_]), this->gain_, this->trigger_base_time_ms_,
           this->trigger_keep_time_ms_, this->protect_time_ms_, this->self_check_time_ms_);

  // The second half of each table repeats the register pattern with the high bit set
  const bool high_freq = this->freq_index_ >= 8;
  const bool high_power = this->power_index_ >= 8;
  const bool ok = this->write_byte(FREQ_MODE_ADDR, FREQ_MODE_VALUE) &&
                  this->write_byte(FREQ_LO_ADDR, 0x40 | (this->freq_index_ & 0x07)) &&
                  this->write_byte(FREQ_HI_ADDR, high_freq ? 0x9e : 0x9d) &&
                  this->write_le_(SIGNAL_DETECTION_THRESHOLD_ADDR, this->delta_, 2) &&
                  this->write_byte(POWER_LO_ADDR, PWR_THRESH_VAL_EN | PWR_WORK_TIME_EN | (this->power_index_ & 0x07)) &&
                  this->write_byte(POWER_HI_ADDR, PWR_BURST_TIME_EN | PWR_THRESH_EN | (high_power ? 0x18 : 0x08)) &&
                  this->write_byte(GAIN_LEVEL1_ADDR, (this->gain_ << 4) | 0x08) &&
                  this->write_byte(GAIN_LEVEL2_ADDR, this->gain_ >> 1) &&
                  this->write_le_(TRIGGER_BASE_TIME_ADDR, this->trigger_base_time_ms_, 4) &&
                  this->write_le_(TRIGGER_KEEP_TIME_ADDR, this->trigger_keep_time_ms_, 4) &&
                  this->write_le_(PROTECT_TIME_ADDR, this->protect_time_ms_, 2) &&
                  this->write_le_(SELF_CHECK_TIME_ADDR, this->self_check_time_ms_, 2) &&
                  this->write_byte(TIME_ENABLE_ADDR, 1) && this->write_byte(ENABLE_ADDR, 0x04);
  // Reset the chip so it uses the new config
  return this->status_comm_result(ok) && this->reset_hardware_frontend();
}

bool AT581XComponent::reset_hardware_frontend() {
  return this->status_comm_result(this->write_registers(RESET_SEQUENCE));
}

bool AT581XComponent::set_rf_mode(bool enable) {
  return this->status_comm_result(enable ? this->write_registers(RF_ON) : this->write_registers(RF_OFF));
}

}  // namespace esphome::at581x
