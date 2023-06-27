#include "as7341.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace as7341 {

static const char *const TAG = "as7341";

void AS7341Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AS7341...");
  LOG_I2C_DEVICE(this);

  // Verify device ID
  uint8_t id;
  this->read_byte(AS7341_ID, &id);
  ESP_LOGCONFIG(TAG, "  Read ID: 0x%X", id);
  if ((id & 0xFC) != (AS7341_CHIP_ID << 2)) {
    this->mark_failed();
    return;
  }

  // Power on (enter IDLE state)
  if (!this->enable_power(true)) {
    ESP_LOGE(TAG, "  Power on failed!");
    this->mark_failed();
    return;
  }

  // Set configuration
  this->write_byte(AS7341_CONFIG, 0x00);
  this->setup_atime(this->atime_);
  this->setup_astep(this->astep_);
  this->setup_gain(this->gain_);
}

void AS7341Component::dump_config() {
  ESP_LOGCONFIG(TAG, "AS7341:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with AS7341 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Gain: %u", get_gain());
  ESP_LOGCONFIG(TAG, "  ATIME: %u", get_atime());
  ESP_LOGCONFIG(TAG, "  ASTEP: %u", get_astep());

  LOG_SENSOR("  ", "F1", this->f1_);
  LOG_SENSOR("  ", "F2", this->f2_);
  LOG_SENSOR("  ", "F3", this->f3_);
  LOG_SENSOR("  ", "F4", this->f4_);
  LOG_SENSOR("  ", "F5", this->f5_);
  LOG_SENSOR("  ", "F6", this->f6_);
  LOG_SENSOR("  ", "F7", this->f7_);
  LOG_SENSOR("  ", "F8", this->f8_);
  LOG_SENSOR("  ", "Clear", this->clear_);
  LOG_SENSOR("  ", "NIR", this->nir_);
}

float AS7341Component::get_setup_priority() const { return setup_priority::DATA; }

void AS7341Component::update() {
  this->read_channels(this->channel_readings_);

  if (this->f1_ != nullptr) {
    this->f1_->publish_state(this->channel_readings_[0]);
  }
  if (this->f2_ != nullptr) {
    this->f2_->publish_state(this->channel_readings_[1]);
  }
  if (this->f3_ != nullptr) {
    this->f3_->publish_state(this->channel_readings_[2]);
  }
  if (this->f4_ != nullptr) {
    this->f4_->publish_state(this->channel_readings_[3]);
  }
  if (this->f5_ != nullptr) {
    this->f5_->publish_state(this->channel_readings_[6]);
  }
  if (this->f6_ != nullptr) {
    this->f6_->publish_state(this->channel_readings_[7]);
  }
  if (this->f7_ != nullptr) {
    this->f7_->publish_state(this->channel_readings_[8]);
  }
  if (this->f8_ != nullptr) {
    this->f8_->publish_state(this->channel_readings_[9]);
  }
  if (this->clear_ != nullptr) {
    this->clear_->publish_state(this->channel_readings_[10]);
  }
  if (this->nir_ != nullptr) {
    this->nir_->publish_state(this->channel_readings_[11]);
  }
}

AS7341Gain AS7341Component::get_gain() {
  uint8_t data;
  this->read_byte(AS7341_CFG1, &data);
  return (AS7341Gain) data;
}

uint8_t AS7341Component::get_atime() {
  uint8_t data;
  this->read_byte(AS7341_ATIME, &data);
  return data;
}

uint16_t AS7341Component::get_astep() {
  uint16_t data;
  this->read_byte_16(AS7341_ASTEP, &data);
  return this->swap_bytes(data);
}

bool AS7341Component::setup_gain(AS7341Gain gain) { return this->write_byte(AS7341_CFG1, gain); }

bool AS7341Component::setup_atime(uint8_t atime) { return this->write_byte(AS7341_ATIME, atime); }

bool AS7341Component::setup_astep(uint16_t astep) { return this->write_byte_16(AS7341_ASTEP, swap_bytes(astep)); }

bool AS7341Component::read_channels(uint16_t *data) {
  this->set_smux_low_channels(true);
  this->enable_spectral_measurement(true);
  this->wait_for_data();
  bool low_success = this->read_bytes_16(AS7341_CH0_DATA_L, data, 6);

  this->set_smux_low_channels(false);
  this->enable_spectral_measurement(true);
  this->wait_for_data();
  bool high_sucess = this->read_bytes_16(AS7341_CH0_DATA_L, &data[6], 6);

  return low_success && high_sucess;
}

void AS7341Component::set_smux_low_channels(bool enable) {
  this->enable_spectral_measurement(false);
  this->set_smux_command(AS7341_SMUX_CMD_WRITE);

  if (enable) {
    this->configure_smux_low_channels();

  } else {
    this->configure_smux_high_channels();
  }
  this->enable_smux();
}

bool AS7341Component::set_smux_command(AS7341SmuxCommand command) {
  uint8_t data = command << 3;  // Write to bits 4:3 of the register
  return this->write_byte(AS7341_CFG6, data);
}

void AS7341Component::configure_smux_low_channels() {
  // SMUX Config for F1,F2,F3,F4,NIR,Clear
  this->write_byte(0x00, 0x30);  // F3 left set to ADC2
  this->write_byte(0x01, 0x01);  // F1 left set to ADC0
  this->write_byte(0x02, 0x00);  // Reserved or disabled
  this->write_byte(0x03, 0x00);  // F8 left disabled
  this->write_byte(0x04, 0x00);  // F6 left disabled
  this->write_byte(0x05, 0x42);  // F4 left connected to ADC3/f2 left connected to ADC1
  this->write_byte(0x06, 0x00);  // F5 left disbled
  this->write_byte(0x07, 0x00);  // F7 left disbled
  this->write_byte(0x08, 0x50);  // CLEAR connected to ADC4
  this->write_byte(0x09, 0x00);  // F5 right disabled
  this->write_byte(0x0A, 0x00);  // F7 right disabled
  this->write_byte(0x0B, 0x00);  // Reserved or disabled
  this->write_byte(0x0C, 0x20);  // F2 right connected to ADC1
  this->write_byte(0x0D, 0x04);  // F4 right connected to ADC3
  this->write_byte(0x0E, 0x00);  // F6/F8 right disabled
  this->write_byte(0x0F, 0x30);  // F3 right connected to AD2
  this->write_byte(0x10, 0x01);  // F1 right connected to AD0
  this->write_byte(0x11, 0x50);  // CLEAR right connected to AD4
  this->write_byte(0x12, 0x00);  // Reserved or disabled
  this->write_byte(0x13, 0x06);  // NIR connected to ADC5
}

void AS7341Component::configure_smux_high_channels() {
  // SMUX Config for F5,F6,F7,F8,NIR,Clear
  this->write_byte(0x00, 0x00);  // F3 left disable
  this->write_byte(0x01, 0x00);  // F1 left disable
  this->write_byte(0x02, 0x00);  // reserved/disable
  this->write_byte(0x03, 0x40);  // F8 left connected to ADC3
  this->write_byte(0x04, 0x02);  // F6 left connected to ADC1
  this->write_byte(0x05, 0x00);  // F4/ F2 disabled
  this->write_byte(0x06, 0x10);  // F5 left connected to ADC0
  this->write_byte(0x07, 0x03);  // F7 left connected to ADC2
  this->write_byte(0x08, 0x50);  // CLEAR Connected to ADC4
  this->write_byte(0x09, 0x10);  // F5 right connected to ADC0
  this->write_byte(0x0A, 0x03);  // F7 right connected to ADC2
  this->write_byte(0x0B, 0x00);  // Reserved or disabled
  this->write_byte(0x0C, 0x00);  // F2 right disabled
  this->write_byte(0x0D, 0x00);  // F4 right disabled
  this->write_byte(0x0E, 0x24);  // F8 right connected to ADC2/ F6 right connected to ADC1
  this->write_byte(0x0F, 0x00);  // F3 right disabled
  this->write_byte(0x10, 0x00);  // F1 right disabled
  this->write_byte(0x11, 0x50);  // CLEAR right connected to AD4
  this->write_byte(0x12, 0x00);  // Reserved or disabled
  this->write_byte(0x13, 0x06);  // NIR connected to ADC5
}

bool AS7341Component::enable_smux() {
  this->set_register_bit(AS7341_ENABLE, 4);

  uint16_t timeout = 1000;
  for (uint16_t time = 0; time < timeout; time++) {
    // The SMUXEN bit is cleared once the SMUX operation is finished
    bool smuxen = this->read_register_bit(AS7341_ENABLE, 4);
    if (!smuxen) {
      return true;
    }

    delay(1);
  }

  return false;
}

bool AS7341Component::wait_for_data() {
  uint16_t timeout = 1000;
  for (uint16_t time = 0; time < timeout; time++) {
    if (this->is_data_ready()) {
      return true;
    }

    delay(1);
  }

  return false;
}

bool AS7341Component::is_data_ready() { return this->read_register_bit(AS7341_STATUS2, 6); }

bool AS7341Component::enable_power(bool enable) { return this->write_register_bit(AS7341_ENABLE, enable, 0); }

bool AS7341Component::enable_spectral_measurement(bool enable) {
  return this->write_register_bit(AS7341_ENABLE, enable, 1);
}

bool AS7341Component::read_register_bit(uint8_t address, uint8_t bit_position) {
  uint8_t data;
  this->read_byte(address, &data);
  bool bit = (data & (1 << bit_position)) > 0;
  return bit;
}

bool AS7341Component::write_register_bit(uint8_t address, bool value, uint8_t bit_position) {
  if (value) {
    return this->set_register_bit(address, bit_position);
  }

  return this->clear_register_bit(address, bit_position);
}

bool AS7341Component::set_register_bit(uint8_t address, uint8_t bit_position) {
  uint8_t data;
  this->read_byte(address, &data);
  data |= (1 << bit_position);
  return this->write_byte(address, data);
}

bool AS7341Component::clear_register_bit(uint8_t address, uint8_t bit_position) {
  uint8_t data;
  this->read_byte(address, &data);
  data &= ~(1 << bit_position);
  return this->write_byte(address, data);
}

uint16_t AS7341Component::swap_bytes(uint16_t data) { return (data >> 8) | (data << 8); }

}  // namespace as7341
}  // namespace esphome
