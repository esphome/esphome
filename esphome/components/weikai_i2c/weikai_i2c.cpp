/// @file weikai_i2c.cpp
/// @brief  WeiKai component family - classes implementation
/// @date Last Modified: 2024/04/06 14:43:31
/// @details The classes declared in this file can be used by the Weikai family

#include "weikai_i2c.h"

namespace esphome {
namespace weikai_i2c {
static const char *const TAG = "weikai_i2c";

/// @brief Display a buffer in hexadecimal format (32 hex values / line).
void print_buffer(const uint8_t *data, size_t length) {
  char hex_buffer[100];
  hex_buffer[(3 * 32) + 1] = 0;
  for (size_t i = 0; i < length; i++) {
    snprintf(&hex_buffer[3 * (i % 32)], sizeof(hex_buffer), "%02X ", data[i]);
    if (i % 32 == 31) {
      ESP_LOGVV(TAG, "   %s", hex_buffer);
    }
  }
  if (length % 32) {
    // null terminate if incomplete line
    hex_buffer[3 * (length % 32) + 2] = 0;
    ESP_LOGVV(TAG, "   %s", hex_buffer);
  }
}

static const char *const REG_TO_STR_P0[16] = {"GENA", "GRST",  "GMUT",  "SPAGE", "SCR", "LCR",  "FCR",  "SIER",
                                              "SIFR", "TFCNT", "RFCNT", "FSR",   "LSR", "FDAT", "FWCR", "RS485"};
static const char *const REG_TO_STR_P1[16] = {"GENA", "GRST", "GMUT", "SPAGE", "BAUD1", "BAUD0", "PRES", "RFTL",
                                              "TFTL", "FWTH", "FWTL", "XON1",  "XOFF1", "SADR",  "SAEN", "RTSDLY"};
using namespace weikai;
// method to print a register value as text: used in the log messages ...
const char *reg_to_str(int reg, bool page1) {
  if (reg == WKREG_GPDAT) {
    return "GPDAT";
  } else if (reg == WKREG_GPDIR) {
    return "GPDIR";
  } else {
    return page1 ? REG_TO_STR_P1[reg & 0x0F] : REG_TO_STR_P0[reg & 0x0F];
  }
}
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

///////////////////////////////////////////////////////////////////////////////
// The WeikaiRegisterI2C methods
///////////////////////////////////////////////////////////////////////////////
uint8_t WeikaiRegisterI2C::read_reg() const {
  uint8_t value = 0x00;
  WeikaiComponentI2C *comp_i2c = static_cast<WeikaiComponentI2C *>(this->comp_);
  uint8_t address = i2c_address(comp_i2c->base_address_, this->channel_, REG);
  comp_i2c->set_i2c_address(address);
  auto error = comp_i2c->read_register(this->register_, &value, 1);
  if (error == i2c::NO_ERROR) {
    this->comp_->status_clear_warning();
    ESP_LOGVV(TAG, "WeikaiRegisterI2C::read_reg() @%02X reg=%s ch=%u I2C_code:%d, buf=%02X", address,
              reg_to_str(this->register_, comp_i2c->page1()), this->channel_, (int) error, value);
  } else {  // error
    this->comp_->status_set_warning();
    ESP_LOGE(TAG, "WeikaiRegisterI2C::read_reg() @%02X reg=%s ch=%u I2C_code:%d, buf=%02X", address,
             reg_to_str(this->register_, comp_i2c->page1()), this->channel_, (int) error, value);
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
    print_buffer(data, length);
#endif
  } else {  // error
    this->comp_->status_set_warning();
    ESP_LOGE(TAG, "WeikaiRegisterI2C::read_fifo() @%02X reg=N/A ch=%d I2C_code:%d len=%d buf=%02X...", address,
             this->channel_, (int) error, length, data[0]);
  }
}

void WeikaiRegisterI2C::write_reg(uint8_t value) {
  WeikaiComponentI2C *comp_i2c = static_cast<WeikaiComponentI2C *>(this->comp_);
  uint8_t address = i2c_address(comp_i2c->base_address_, this->channel_, REG);  // update the i2c bus
  comp_i2c->set_i2c_address(address);
  auto error = comp_i2c->write_register(this->register_, &value, 1);
  if (error == i2c::NO_ERROR) {
    this->comp_->status_clear_warning();
    ESP_LOGVV(TAG, "WK2168Reg::write_reg() @%02X reg=%s ch=%d I2C_code:%d buf=%02X", address,
              reg_to_str(this->register_, comp_i2c->page1()), this->channel_, (int) error, value);
  } else {  // error
    this->comp_->status_set_warning();
    ESP_LOGE(TAG, "WK2168Reg::write_reg() @%02X reg=%s ch=%d I2C_code:%d buf=%d", address,
             reg_to_str(this->register_, comp_i2c->page1()), this->channel_, (int) error, value);
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
    print_buffer(data, length);
#endif
  } else {  // error
    this->comp_->status_set_warning();
    ESP_LOGE(TAG, "WK2168Reg::write_fifo() @%02X reg=N/A, ch=%d I2C_code:%d len=%d, buf=%02X...", address,
             this->channel_, (int) error, length, data[0]);
  }
}

///////////////////////////////////////////////////////////////////////////////
// The WeikaiComponentI2C methods
///////////////////////////////////////////////////////////////////////////////
void WeikaiComponentI2C::setup() {
  // before any manipulation we store the address to base_address_ for future use
  this->base_address_ = this->address_;
  ESP_LOGCONFIG(TAG, "Setting up wk2168_i2c: %s with %d UARTs at @%02X ...", this->get_name(), this->children_.size(),
                this->base_address_);

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
  ESP_LOGCONFIG(TAG, "Initialization of %s with %d UARTs completed", this->get_name(), this->children_.size());
  ESP_LOGCONFIG(TAG, "  Crystal: %" PRIu32, this->crystal_);
  if (test_mode_)
    ESP_LOGCONFIG(TAG, "  Test mode: %d", test_mode_);
  ESP_LOGCONFIG(TAG, "  Transfer buffer size: %d", XFER_MAX_SIZE);
  this->address_ = this->base_address_;  // we restore the base_address before display (less confusing)
  LOG_I2C_DEVICE(this);

  for (auto *child : this->children_) {
    child->dump_channel();
  }
}

}  // namespace weikai_i2c
}  // namespace esphome
