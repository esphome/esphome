/// @file weikai_i2c.cpp
/// @brief  WeiKai component family - classes implementation
/// @date Last Modified: 2024/04/06 14:43:31
/// @details The classes declared in this file can be used by the Weikai family

#include "weikai_i2c.h"
#include "esphome/core/hal.h"

namespace esphome::weikai_i2c {
ESPHOME_LOG_TAG(TAG, "weikai_i2c");

using namespace weikai;
enum RegType { REG = 0, FIFO = 1 };  ///< Register or FIFO

/// @brief Computes the I²C bus's address used to access the component
/// @param base_address the base address of the component - set by the A1 A0 pins
/// @param channel (0-3) the UART channel
/// @param fifo (0-1) 0 = access to internal register, 1 = direct access to fifo
/// @return the i2c address to use
inline uint8_t i2c_address(uint8_t base_address, uint8_t channel, RegType fifo) {
  // the address of the device is:
  // +----+----+----+----+----+----+----+----+
  // |  0 | A1 | A0 |  1 |  0 | C1 | C0 |  F |
  // +----+----+----+----+----+----+----+----+
  // where:
  // - A1,A0 is the address read from A1,A0 switch
  // - C1,C0 is the channel number (in practice only 00 or 01)
  // - F is: 0 when accessing register, one when accessing FIFO
  uint8_t const addr = base_address | channel << 1 | fifo << 0;
  return addr;
}

/// @brief Splits a register number into the channel bits and the 4 bit register address
/// @details On the I²C bus the register byte only carries the 4 bit address A3-A0; the two
/// bits C1,C0 of the 6 bit address travel in the device address. This also holds for the
/// global registers: GPDIR (0x21) is register 1 at channel bits 10 and GPDAT (0x31) is
/// register 1 at channel bits 11. Sent as is to the channel 0 address, they are taken as
/// GRST (register 1) and soft-reset the UART channels.
/// @param reg the register number, the bits 5-4 hold C1,C0 for the global GPIO registers
/// @param channel in: the channel of the register, out: the channel bits to put in the address
/// @return the 4 bit register address
inline uint8_t i2c_register(uint8_t reg, uint8_t &channel) {
  if (reg > 0x0F) {
    channel = (reg >> 4) & 0x03;
  }
  return reg & 0x0F;
}

///////////////////////////////////////////////////////////////////////////////
// The WeikaiRegisterI2C methods
///////////////////////////////////////////////////////////////////////////////
uint8_t WeikaiRegisterI2C::read_reg() const {
  uint8_t value = 0x00;
  WeikaiComponentI2C *comp_i2c = static_cast<WeikaiComponentI2C *>(this->comp_);
  uint8_t channel = this->channel_;
  uint8_t const reg = i2c_register(this->register_, channel);
  uint8_t address = i2c_address(comp_i2c->base_address_, channel, REG);
  comp_i2c->set_i2c_address(address);
  // A register is read in two transfers: the register address is written and closed with
  // a STOP, then the value is read. A read after a repeated start is not always
  // acknowledged. An occasional transfer is still not acknowledged, so it is tried a
  // second time. The retry is limited to the cases where no data byte has left the chip
  // (the register address write failed, or the read was not acknowledged), so that a
  // clear-on-read status register such as LSR or SIFR is never read twice.
  i2c::ErrorCode error;
  for (uint8_t attempt = 0;; attempt++) {
    bool data_phase = false;
    error = comp_i2c->write(&reg, 1);
    if (error == i2c::NO_ERROR) {
      error = comp_i2c->read(&value, 1);
      data_phase = error != i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    if (error == i2c::NO_ERROR || data_phase || attempt == 1)
      break;
    ESP_LOGV(TAG, "WeikaiRegisterI2C::read_reg() @%02X reg=%s ch=%u I2C_code:%d, retrying", address,
             LOG_STR_ARG(reg_to_str(this->register_, comp_i2c->page1())), this->channel_, (int) error);
    delayMicroseconds(100);
  }
  if (error == i2c::NO_ERROR) {
    this->comp_->status_clear_warning();
    ESP_LOGVV(TAG, "WeikaiRegisterI2C::read_reg() @%02X reg=%s ch=%u I2C_code:%d, buf=%02X", address,
              LOG_STR_ARG(reg_to_str(this->register_, comp_i2c->page1())), this->channel_, (int) error, value);
  } else {  // error
    this->comp_->status_set_warning();
    ESP_LOGE(TAG, "WeikaiRegisterI2C::read_reg() @%02X reg=%s ch=%u I2C_code:%d, buf=%02X", address,
             LOG_STR_ARG(reg_to_str(this->register_, comp_i2c->page1())), this->channel_, (int) error, value);
  }
  return value;
}

void WeikaiRegisterI2C::read_fifo(uint8_t *data, size_t length) const {
  WeikaiComponentI2C *comp_i2c = static_cast<WeikaiComponentI2C *>(this->comp_);
  uint8_t address = i2c_address(comp_i2c->base_address_, this->channel_, FIFO);
  comp_i2c->set_i2c_address(address);
  auto error = comp_i2c->read(data, length);
  if (error == i2c::NO_ERROR) {
    this->comp_->status_clear_warning();
#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
    ESP_LOGVV(TAG, "WeikaiRegisterI2C::read_fifo() @%02X ch=%d I2C_code:%d len=%d buffer", address, this->channel_,
              (int) error, length);
    print_buffer(TAG, data, length);
#endif
  } else {  // error
    this->comp_->status_set_warning();
    ESP_LOGE(TAG, "WeikaiRegisterI2C::read_fifo() @%02X reg=N/A ch=%d I2C_code:%d len=%d buf=%02X", address,
             this->channel_, (int) error, length, data[0]);
  }
}

void WeikaiRegisterI2C::write_reg(uint8_t value) {
  WeikaiComponentI2C *comp_i2c = static_cast<WeikaiComponentI2C *>(this->comp_);
  uint8_t channel = this->channel_;
  uint8_t const reg = i2c_register(this->register_, channel);
  uint8_t address = i2c_address(comp_i2c->base_address_, channel, REG);  // update the i2c bus
  comp_i2c->set_i2c_address(address);
  auto error = comp_i2c->write_register(reg, &value, 1);
  if (error == i2c::NO_ERROR) {
    this->comp_->status_clear_warning();
    ESP_LOGVV(TAG, "WK2168Reg::write_reg() @%02X reg=%s ch=%d I2C_code:%d buf=%02X", address,
              LOG_STR_ARG(reg_to_str(this->register_, comp_i2c->page1())), this->channel_, (int) error, value);
  } else {  // error
    this->comp_->status_set_warning();
    ESP_LOGE(TAG, "WK2168Reg::write_reg() @%02X reg=%s ch=%d I2C_code:%d buf=%d", address,
             LOG_STR_ARG(reg_to_str(this->register_, comp_i2c->page1())), this->channel_, (int) error, value);
  }
}

void WeikaiRegisterI2C::write_fifo(uint8_t *data, size_t length) {
  WeikaiComponentI2C *comp_i2c = static_cast<WeikaiComponentI2C *>(this->comp_);
  uint8_t address = i2c_address(comp_i2c->base_address_, this->channel_, FIFO);  // set fifo flag
  comp_i2c->set_i2c_address(address);
  auto error = comp_i2c->write(data, length);
  if (error == i2c::NO_ERROR) {
    this->comp_->status_clear_warning();
#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
    ESP_LOGVV(TAG, "WK2168Reg::write_fifo() @%02X ch=%d I2C_code:%d len=%d buffer", address, this->channel_,
              (int) error, length);
    print_buffer(TAG, data, length);
#endif
  } else {  // error
    this->comp_->status_set_warning();
    ESP_LOGE(TAG, "WK2168Reg::write_fifo() @%02X reg=N/A, ch=%d I2C_code:%d len=%d, buf=%02X", address, this->channel_,
             (int) error, length, data[0]);
  }
}

///////////////////////////////////////////////////////////////////////////////
// The WeikaiComponentI2C methods
///////////////////////////////////////////////////////////////////////////////
void WeikaiComponentI2C::setup() {
  // before any manipulation we store the address to base_address_ for future use
  this->base_address_ = this->address_;
  ESP_LOGCONFIG(TAG, "Setup %s (%d UARTs) @ 0x%02X", this->get_name(), this->children_.size(), this->base_address_);

  // enable all channels
  this->reg(WKREG_GENA, 0) = GENA_C1EN | GENA_C2EN | GENA_C3EN | GENA_C4EN;
  // reset all channels
  this->reg(WKREG_GRST, 0) = GRST_C1RST | GRST_C2RST | GRST_C3RST | GRST_C4RST;
  // initialize the spage register to page 0
  this->reg(WKREG_SPAGE, 0) = 0;
  this->page1_ = false;

  // we setup our children channels
  for (auto *child : this->children_) {
    child->setup_channel();
  }
}

void WeikaiComponentI2C::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Initialization of %s with %d UARTs completed\n"
                "  Crystal: %" PRIu32,
                this->get_name(), this->children_.size(), this->crystal_);
  if (test_mode_) {
    ESP_LOGCONFIG(TAG,
                  "  Test mode: %d\n"
                  "  Transfer buffer size: %d",
                  test_mode_, XFER_MAX_SIZE);
  }
  this->address_ = this->base_address_;  // we restore the base_address before display (less confusing)
  LOG_I2C_DEVICE(this);

  for (auto *child : this->children_) {
    child->dump_channel();
  }
}

}  // namespace esphome::weikai_i2c
