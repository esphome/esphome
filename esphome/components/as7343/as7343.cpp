#include "as7343.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace as7343 {

static const char *const TAG = "as7343";

void AS7343Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AS7343...");
  LOG_I2C_DEVICE(this);

  // Verify device ID
  this->set_bank_for_reg_(AS7343Registers::ID);
  uint8_t id = this->reg((uint8_t) AS7343Registers::ID).get();
  ESP_LOGCONFIG(TAG, "  Read ID: 0x%X", id);
  if (id != AS7343_CHIP_ID) {
    this->mark_failed();
    ESP_LOGE(TAG, "  Invalid chip ID: 0x%X", id);
    return;
  }

  this->set_bank_for_reg_(AS7343Registers::ENABLE);
  // Power on (enter IDLE state)
  if (!this->enable_power(true)) {
    ESP_LOGE(TAG, "  Power on failed!");
    this->mark_failed();
    return;
  }

  // Set configuration
  AS7343RegCfg20 cfg20;
  cfg20.raw = this->reg((uint8_t) AS7343Registers::CFG20).get();
  cfg20.auto_smux = 0b11;
  this->reg((uint8_t) AS7343Registers::CFG20) = cfg20.raw;

  this->setup_atime(this->atime_);
  this->setup_astep(this->astep_);
  this->setup_gain(this->gain_);

  // enable led false ?
}

void AS7343Component::dump_config() {
  ESP_LOGCONFIG(TAG, "AS7343:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with AS7343 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Gain: %u", get_gain());
  ESP_LOGCONFIG(TAG, "  ATIME: %u", get_atime());
  ESP_LOGCONFIG(TAG, "  ASTEP: %u", get_astep());

  LOG_SENSOR("  ", "F1", this->f1_);
  LOG_SENSOR("  ", "F2", this->f2_);
  LOG_SENSOR("  ", "FZ", this->fz_);
  LOG_SENSOR("  ", "F3", this->f3_);
  LOG_SENSOR("  ", "F4", this->f4_);
  LOG_SENSOR("  ", "F5", this->f5_);
  LOG_SENSOR("  ", "FY", this->fy_);
  LOG_SENSOR("  ", "FXL", this->fxl_);
  LOG_SENSOR("  ", "F6", this->f6_);
  LOG_SENSOR("  ", "F7", this->f7_);
  LOG_SENSOR("  ", "F8", this->f8_);
  LOG_SENSOR("  ", "NIR", this->nir_);
  LOG_SENSOR("  ", "Clear", this->clear_);
}

float AS7343Component::get_setup_priority() const { return setup_priority::DATA; }

void AS7343Component::update() {
  this->read_channels(this->channel_readings_);

  if (this->f1_ != nullptr) {
    this->f1_->publish_state(this->channel_readings_[AS7343_CHANNEL_405_F1]);
  }
  if (this->f2_ != nullptr) {
    this->f2_->publish_state(this->channel_readings_[AS7343_CHANNEL_425_F2]);
  }
  if (this->fz_ != nullptr) {
    this->fz_->publish_state(this->channel_readings_[AS7343_CHANNEL_450_FZ]);
  }
  if (this->f3_ != nullptr) {
    this->f3_->publish_state(this->channel_readings_[AS7343_CHANNEL_475_F3]);
  }
  if (this->f4_ != nullptr) {
    this->f4_->publish_state(this->channel_readings_[AS7343_CHANNEL_515_F4]);
  }
  if (this->fy_ != nullptr) {
    this->fy_->publish_state(this->channel_readings_[AS7343_CHANNEL_555_FY]);
  }
  if (this->f5_ != nullptr) {
    this->f5_->publish_state(this->channel_readings_[AS7343_CHANNEL_550_F5]);
  }
  if (this->fxl_ != nullptr) {
    this->fxl_->publish_state(this->channel_readings_[AS7343_CHANNEL_600_FXL]);
  }
  if (this->f6_ != nullptr) {
    this->f6_->publish_state(this->channel_readings_[AS7343_CHANNEL_640_F6]);
  }
  if (this->f7_ != nullptr) {
    this->f7_->publish_state(this->channel_readings_[AS7343_CHANNEL_690_F7]);
  }
  if (this->f8_ != nullptr) {
    this->f8_->publish_state(this->channel_readings_[AS7343_CHANNEL_745_F8]);
  }
  if (this->nir_ != nullptr) {
    this->nir_->publish_state(this->channel_readings_[AS7343_CHANNEL_855_NIR]);
  }
  if (this->clear_ != nullptr) {
    this->clear_->publish_state(this->channel_readings_[AS7343_CHANNEL_CLEAR]);
  }
}

AS7343Gain AS7343Component::get_gain() {
  uint8_t data;
  this->read_byte((uint8_t) AS7343Registers::CFG1, &data);
  return (AS7343Gain) data;
}

uint8_t AS7343Component::get_atime() {
  uint8_t data;
  this->read_byte((uint8_t) AS7343Registers::ATIME, &data);
  return data;
}

uint16_t AS7343Component::get_astep() {
  uint16_t data;
  this->read_byte_16((uint8_t) AS7343Registers::ASTEP_LSB, &data);
  return this->swap_bytes(data);
}

bool AS7343Component::setup_gain(AS7343Gain gain) { return this->write_byte((uint8_t) AS7343Registers::CFG1, gain); }

bool AS7343Component::setup_atime(uint8_t atime) { return this->write_byte((uint8_t) AS7343Registers::ATIME, atime); }

bool AS7343Component::setup_astep(uint16_t astep) {
  return this->write_byte_16((uint8_t) AS7343Registers::ASTEP_LSB, swap_bytes(astep));
}

bool AS7343Component::read_channels(uint16_t *data) {
  this->enable_spectral_measurement(true);
  this->wait_for_data();

  return this->read_bytes_16((uint8_t) AS7343Registers::DATA_O, this->channel_readings_, AS7343_NUM_CHANNELS);

  // this->set_smux_low_channels(true);
  // this->enable_spectral_measurement(true);
  // this->wait_for_data();
  // bool low_success = this->read_bytes_16(AS7343_CH0_DATA_L, data, 6);

  // this->set_smux_low_channels(false);
  // this->enable_spectral_measurement(true);
  // this->wait_for_data();
  // bool high_sucess = this->read_bytes_16(AS7343_CH0_DATA_L, &data[6], 6);

  // return low_success && high_sucess;
}

// void AS7343Component::set_smux_low_channels(bool enable) {
//   // this->enable_spectral_measurement(false);
//   // this->set_smux_command(AS7343_SMUX_CMD_WRITE);

//   // if (enable) {
//   //   this->configure_smux_low_channels();

//   // } else {
//   //   this->configure_smux_high_channels();
//   // }
//   // this->enable_smux();
// }

// bool AS7343Component::set_smux_command(AS7343SmuxCommand command) {
//   // uint8_t data = command << 3;  // Write to bits 4:3 of the register
//   // return this->write_byte(AS7343_CFG6, data);
// }

// void AS7343Component::configure_smux_low_channels() {
//   // SMUX Config for F1,F2,F3,F4,NIR,Clear
//   // this->write_byte(0x00, 0x30);  // F3 left set to ADC2
//   // this->write_byte(0x01, 0x01);  // F1 left set to ADC0
//   // this->write_byte(0x02, 0x00);  // Reserved or disabled
//   // this->write_byte(0x03, 0x00);  // F8 left disabled
//   // this->write_byte(0x04, 0x00);  // F6 left disabled
//   // this->write_byte(0x05, 0x42);  // F4 left connected to ADC3/f2 left connected to ADC1
//   // this->write_byte(0x06, 0x00);  // F5 left disbled
//   // this->write_byte(0x07, 0x00);  // F7 left disbled
//   // this->write_byte(0x08, 0x50);  // CLEAR connected to ADC4
//   // this->write_byte(0x09, 0x00);  // F5 right disabled
//   // this->write_byte(0x0A, 0x00);  // F7 right disabled
//   // this->write_byte(0x0B, 0x00);  // Reserved or disabled
//   // this->write_byte(0x0C, 0x20);  // F2 right connected to ADC1
//   // this->write_byte(0x0D, 0x04);  // F4 right connected to ADC3
//   // this->write_byte(0x0E, 0x00);  // F6/F8 right disabled
//   // this->write_byte(0x0F, 0x30);  // F3 right connected to AD2
//   // this->write_byte(0x10, 0x01);  // F1 right connected to AD0
//   // this->write_byte(0x11, 0x50);  // CLEAR right connected to AD4
//   // this->write_byte(0x12, 0x00);  // Reserved or disabled
//   // this->write_byte(0x13, 0x06);  // NIR connected to ADC5
// }

// void AS7343Component::configure_smux_high_channels() {
//   // SMUX Config for F5,F6,F7,F8,NIR,Clear
//   // this->write_byte(0x00, 0x00);  // F3 left disable
//   // this->write_byte(0x01, 0x00);  // F1 left disable
//   // this->write_byte(0x02, 0x00);  // reserved/disable
//   // this->write_byte(0x03, 0x40);  // F8 left connected to ADC3
//   // this->write_byte(0x04, 0x02);  // F6 left connected to ADC1
//   // this->write_byte(0x05, 0x00);  // F4/ F2 disabled
//   // this->write_byte(0x06, 0x10);  // F5 left connected to ADC0
//   // this->write_byte(0x07, 0x03);  // F7 left connected to ADC2
//   // this->write_byte(0x08, 0x50);  // CLEAR Connected to ADC4
//   // this->write_byte(0x09, 0x10);  // F5 right connected to ADC0
//   // this->write_byte(0x0A, 0x03);  // F7 right connected to ADC2
//   // this->write_byte(0x0B, 0x00);  // Reserved or disabled
//   // this->write_byte(0x0C, 0x00);  // F2 right disabled
//   // this->write_byte(0x0D, 0x00);  // F4 right disabled
//   // this->write_byte(0x0E, 0x24);  // F8 right connected to ADC2/ F6 right connected to ADC1
//   // this->write_byte(0x0F, 0x00);  // F3 right disabled
//   // this->write_byte(0x10, 0x00);  // F1 right disabled
//   // this->write_byte(0x11, 0x50);  // CLEAR right connected to AD4
//   // this->write_byte(0x12, 0x00);  // Reserved or disabled
//   // this->write_byte(0x13, 0x06);  // NIR connected to ADC5
// }

// bool AS7343Component::enable_smux() {
//   // this->set_register_bit(AS7343_ENABLE, 4);

//   // uint16_t timeout = 1000;
//   // for (uint16_t time = 0; time < timeout; time++) {
//   //   // The SMUXEN bit is cleared once the SMUX operation is finished
//   //   bool smuxen = this->read_register_bit(AS7343_ENABLE, 4);
//   //   if (!smuxen) {
//   //     return true;
//   //   }

//   //   delay(1);
//   // }

//   // return false;
// }

bool AS7343Component::wait_for_data(uint16_t timeout) {
  for (uint16_t time = 0; time < timeout; time++) {
    if (this->is_data_ready()) {
      return true;
    }

    delay(1);
  }

  return false;
}

bool AS7343Component::is_data_ready() { return this->read_register_bit((uint8_t) AS7343Registers::STATUS2, 6); }

void AS7343Component::set_bank_for_reg_(AS7343Registers reg) {
  bool bank = (uint8_t) reg < 0x80;
  if (bank == this->bank_) {
    return;
  }
  this->write_register_bit((uint8_t) AS7343Registers::CFG0, bank, AS7343_CFG0_REG_BANK_BIT);
  this->bank_ = bank;
}

bool AS7343Component::enable_power(bool enable) {
  return this->write_register_bit((uint8_t) AS7343Registers::ENABLE, enable, AS7343_ENABLE_PON_BIT);
}

bool AS7343Component::enable_spectral_measurement(bool enable) {
  return this->write_register_bit((uint8_t) AS7343Registers::ENABLE, enable, AS7343_ENABLE_SP_EN_BIT);
}

bool AS7343Component::read_register_bit(uint8_t address, uint8_t bit_position) {
  uint8_t data;
  this->read_byte(address, &data);
  bool bit = (data & (1 << bit_position)) > 0;
  return bit;
}

bool AS7343Component::write_register_bit(uint8_t address, bool value, uint8_t bit_position) {
  if (value) {
    return this->set_register_bit(address, bit_position);
  }

  return this->clear_register_bit(address, bit_position);
}

bool AS7343Component::set_register_bit(uint8_t address, uint8_t bit_position) {
  uint8_t data;
  this->read_byte(address, &data);
  data |= (1 << bit_position);
  return this->write_byte(address, data);
}

bool AS7343Component::clear_register_bit(uint8_t address, uint8_t bit_position) {
  uint8_t data;
  this->read_byte(address, &data);
  data &= ~(1 << bit_position);
  return this->write_byte(address, data);
}

uint16_t AS7343Component::swap_bytes(uint16_t data) { return (data >> 8) | (data << 8); }

}  // namespace as7343
}  // namespace esphome
