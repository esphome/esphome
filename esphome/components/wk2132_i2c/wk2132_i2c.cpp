/// @file wk2132_i2c.cpp
/// @author DrCoolzic
/// @brief wk2132 classes implementation

#include "wk2132_i2c.h"
// #include "esphome/components/wk2132/wk2132.h"

namespace esphome {
namespace wk2132_i2c {

static const char *const TAG = "wk2132_i2c";

static const char *const REG_TO_STR_P0[] = {"GENA", "GRST", "GMUT",  "SPAGE", "SCR", "LCR", "FCR",
                                            "SIER", "SIFR", "TFCNT", "RFCNT", "FSR", "LSR", "FDAT"};
static const char *const REG_TO_STR_P1[] = {"GENA", "GRST", "GMUT",  "SPAGE", "BAUD1", "BAUD0", "PRES",
                                            "RFTL", "TFTL", "_INV_", "_INV_", "_INV_", "_INV_"};

// method to print a register value as text: used in the log messages ...
const char *reg_to_str(int reg, bool page1) { return page1 ? REG_TO_STR_P1[reg] : REG_TO_STR_P0[reg]; }

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
  uint8_t const addr = base_address | channel << 1 | fifo;
  return addr;
}

///////////////////////////////////////////////////////////////////////////////
// The WK2132Register methods
///////////////////////////////////////////////////////////////////////////////
uint8_t WK2132RegisterI2C::read_reg() const {
  uint8_t value = 0x00;
  WK2132ComponentI2C *i2c_comp = static_cast<WK2132ComponentI2C *>(this->comp_);
  i2c_comp->address_ = i2c_address(i2c_comp->base_address_, this->channel_, REG);  // update the i2c bus address
  auto error = i2c_comp->read_register(this->register_, &value, 1);
  if (error == i2c::ERROR_OK) {
    this->comp_->status_clear_warning();
    ESP_LOGVV(TAG, "WK2132Register::read_reg() @%02X r=%s, ch=%d b=%02X, I2C_code:%d", i2c_comp->address_,
              reg_to_str(this->register_, i2c_comp->page1_), this->channel_, value, (int) error);
  } else {  // error
    this->comp_->status_set_warning();
    ESP_LOGE(TAG, "WK2132Register::read_reg() @%02X r=%s, ch=%d b=%02X, I2C_code:%d", i2c_comp->address_,
             reg_to_str(this->register_, i2c_comp->page1_), this->channel_, value, (int) error);
  }
  return value;
}

void WK2132RegisterI2C::read_fifo(uint8_t *data, size_t length) const {
  WK2132ComponentI2C *i2c_comp = static_cast<WK2132ComponentI2C *>(this->comp_);
  i2c_comp->address_ = i2c_address(i2c_comp->base_address_, this->channel_, FIFO);
  auto error = i2c_comp->read(data, length);
  if (error == i2c::ERROR_OK) {
    this->comp_->status_clear_warning();
    ESP_LOGVV(TAG, "WK2132Register::read_fifo() @%02X r=%s, ch=%d b=%02X, I2C_code:%d", i2c_comp->address_,
              reg_to_str(this->register_, i2c_comp->page1_), this->channel_, length, (int) error);
  } else {  // error
    this->comp_->status_set_warning();
    ESP_LOGE(TAG, "WK2132Register::read_fifo() @%02X r=%s, ch=%d b=%02X, I2C_code:%d", i2c_comp->address_,
             reg_to_str(this->register_, i2c_comp->page1_), this->channel_, length, (int) error);
  }
}

void WK2132RegisterI2C::write_reg(uint8_t value) {
  WK2132ComponentI2C *i2c_comp = static_cast<WK2132ComponentI2C *>(this->comp_);
  i2c_comp->address_ = i2c_address(i2c_comp->base_address_, this->channel_, REG);  // update the i2c bus
  auto error = i2c_comp->write_register(this->register_, &value, 1);
  if (error == i2c::ERROR_OK) {
    this->comp_->status_clear_warning();
    ESP_LOGVV(TAG, "WK2132Register::write_reg() @%02X r=%s, ch=%d b=%02X, I2C_code:%d", i2c_comp->address_,
              reg_to_str(this->register_, i2c_comp->page1_), this->channel_, value, (int) error);
  } else {  // error
    this->comp_->status_set_warning();
    ESP_LOGE(TAG, "WK2132Register::write_reg() @%02X r=%s, ch=%d b=%02X, I2C_code:%d", i2c_comp->address_,
             reg_to_str(this->register_, i2c_comp->page1_), this->channel_, value, (int) error);
  }
}

void WK2132RegisterI2C::write_fifo(const uint8_t *data, size_t length) {
  WK2132ComponentI2C *i2c_comp = static_cast<WK2132ComponentI2C *>(this->comp_);
  i2c_comp->address_ = i2c_address(i2c_comp->base_address_, this->channel_, FIFO);  // set fifo flag
  auto error = i2c_comp->write(data, length);
  if (error == i2c::ERROR_OK) {
    this->comp_->status_clear_warning();
    ESP_LOGVV(TAG, "WK2132Register::write_fifo() @%02X r=%s, ch=%d b=%02X, I2C_code:%d", i2c_comp->address_,
              reg_to_str(this->register_, i2c_comp->page1_), this->channel_, length, (int) error);
  } else {  // error
    this->comp_->status_set_warning();
    ESP_LOGE(TAG, "WK2132Register::write_fifo() @%02X r=%s, ch=%d b=%02X, I2C_code:%d", i2c_comp->address_,
             reg_to_str(this->register_, i2c_comp->page1_), this->channel_, length, (int) error);
  }
}

///////////////////////////////////////////////////////////////////////////////
// The WK2132Component methods
///////////////////////////////////////////////////////////////////////////////
void WK2132ComponentI2C::setup() {
  using namespace wk2132;
  // before any manipulation we store the address to base_address_ for future use
  this->base_address_ = this->address_;
  ESP_LOGCONFIG(TAG, "Setting up wk2132_i2c: %s with %d UARTs at @%02X ...", this->get_name(), this->children_.size(),
                this->base_address_);

  // enable both channels
  this->global_reg(REG_WK2132_GENA) = GENA_C1EN | GENA_C2EN;
  // reset channels
  this->global_reg(REG_WK2132_GRST) = GRST_C1RST | GRST_C2RST;
  // initialize the spage register to page 0
  this->global_reg(REG_WK2132_SPAGE) = 0;
  this->page1_ = false;

  // we setup our children channels
  for (WK2132Channel *child : this->children_) {
    child->setup_channel();
  }
}

void WK2132ComponentI2C::dump_config() {
  ESP_LOGCONFIG(TAG, "Initialization of %s with %d UARTs completed", this->get_name(), this->children_.size());
  ESP_LOGCONFIG(TAG, "  Crystal: %d", this->crystal_);
  if (test_mode_)
    ESP_LOGCONFIG(TAG, "  Test mode: %d", test_mode_);
  this->address_ = this->base_address_;  // we restore the base_address before display (less confusing)
  LOG_I2C_DEVICE(this);

  for (auto *child : this->children_) {
    child->dump_channel();
  }
}

}  // namespace wk2132_i2c
}  // namespace esphome
