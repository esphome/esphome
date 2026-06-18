#include "as734xbase.h"
#include "esphome/core/log.h"

namespace esphome::as734x {

static const char *const TAG = "as734x.base";

AS734xBase::AS734xBase(i2c::I2CDevice *i2c_device, uint8_t number_of_channels)
    : i2c_device_(i2c_device), number_of_channels_(number_of_channels) {
  ESP_LOGD(TAG, "AS734xBase constructor %p, %d", i2c_device, number_of_channels);
}

bool AS734xBase::write_gain(Gain gain) { return this->i2c_device_->write_byte(this->registers().CFG1, gain); }
bool AS734xBase::write_atime(uint8_t atime) { return this->i2c_device_->write_byte(this->registers().ATIME, atime); }
bool AS734xBase::write_astep(uint16_t astep) {
  return this->i2c_device_->write_byte_16(this->registers().ASTEP, this->swap_bytes_(astep));
}

Gain AS734xBase::read_gain() {
  uint8_t data;
  this->i2c_device_->read_byte(this->registers().CFG1, &data);
  return (Gain) data;
}

uint8_t AS734xBase::read_atime() {
  uint8_t data;
  this->i2c_device_->read_byte(this->registers().ATIME, &data);
  return data;
}

uint16_t AS734xBase::read_astep() {
  uint16_t data;
  this->i2c_device_->read_byte_16(this->registers().ASTEP, &data);
  return this->swap_bytes_(data);
}

bool AS734xBase::enable_power(bool enable) {
  return this->write_register_bit_(this->registers().ENABLE, enable, this->registers().ENABLE_PON_BIT);
}

bool AS734xBase::enable_spectral_measurement(bool enable) {
  return this->write_register_bit_(this->registers().ENABLE, enable, this->registers().ENABLE_SP_EN_BIT);
}

bool AS734xBase::enable_smux() {
  return this->set_register_bit_(this->registers().ENABLE, this->registers().ENABLE_SMUX_EN_BIT);
}

bool AS734xBase::is_smux_ready() {
  return !this->read_register_bit_(this->registers().ENABLE, this->registers().ENABLE_SMUX_EN_BIT);
}

bool AS734xBase::is_data_ready() {
  return this->read_register_bit_(this->registers().STATUS2, this->registers().STATUS2_AVALID_BIT);
}

void AS734xBase::set_bank_(bool low) {
  if (low == this->bank_low_) {
    return;
  }
  this->write_register_bit_(this->registers().CFG0, low, this->registers().CFG0_REG_BANK_BIT);
  this->bank_low_ = low;
}

void AS734xBase::set_bank_for_reg_(uint8_t reg) { this->set_bank_((reg < 0x80)); }

bool AS734xBase::read_register_bit_(uint8_t address, uint8_t bit_position) {
  this->set_bank_for_reg_(address);

  uint8_t data;
  this->i2c_device_->read_byte(address, &data);
  bool bit = (data & (1 << bit_position)) > 0;
  return bit;
}

bool AS734xBase::write_register_bit_(uint8_t address, bool value, uint8_t bit_position) {
  return value ? this->set_register_bit_(address, bit_position) : this->clear_register_bit_(address, bit_position);
}

bool AS734xBase::set_register_bit_(uint8_t address, uint8_t bit_position) {
  this->set_bank_for_reg_(address);
  uint8_t data;
  this->i2c_device_->read_byte(address, &data);
  data |= (1 << bit_position);
  return this->i2c_device_->write_byte(address, data);
}

bool AS734xBase::clear_register_bit_(uint8_t address, uint8_t bit_position) {
  this->set_bank_for_reg_(address);
  uint8_t data;
  this->i2c_device_->read_byte(address, &data);
  data &= ~(1 << bit_position);
  return this->i2c_device_->write_byte(address, data);
}

}  // namespace esphome::as734x
