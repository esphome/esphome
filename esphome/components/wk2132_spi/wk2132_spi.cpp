/// @file wk2132_spi.cpp
/// @author DrCoolzic
/// @brief wk2132 classes implementation

#include "wk2132_spi.h"
// #include "esphome/components/wk2132/wk2132.h"

namespace esphome {
namespace wk2132_spi {

static const char *const TAG = "wk2132_spi";

static const char *const REG_TO_STR_P0[] = {"GENA", "GRST", "GMUT",  "SPAGE", "SCR", "LCR", "FCR",
                                            "SIER", "SIFR", "TFCNT", "RFCNT", "FSR", "LSR", "FDAT"};
static const char *const REG_TO_STR_P1[] = {"GENA", "GRST", "GMUT",  "SPAGE", "BAUD1", "BAUD0", "PRES",
                                            "RFTL", "TFTL", "_INV_", "_INV_", "_INV_", "_INV_"};

// method to print a register value as text: used in the log messages ...
const char *reg_to_str(int reg, bool page1) { return page1 ? REG_TO_STR_P1[reg] : REG_TO_STR_P0[reg]; }

enum TransferType { WRITE = 0, READ = 1 };  ///< Read or Write transfer

/// @brief Computes the SPI command byte
/// @param transfer_type read or write command
/// @param reg the address of the register
/// @param channel (0-3) the UART channel
/// @param fifo (0-1) 0 = access to internal register, 1 = direct access to fifo
/// @return the spi command byte
/// @details
/// +------+------+------+------+------+------+------+------+
/// | FIFO | R/W  |    C1-C0    |           A3-A0           |
/// +------+------+-------------+---------------------------+
inline static uint8_t command_byte(TransferType transfer_type, uint8_t reg, uint8_t channel, uint8_t fifo) {
  return (fifo << 7 | transfer_type << 6 | channel << 4 | reg << 0);
}

/// @brief convert an int to binary representation as C++ std::string
/// @param val integer to convert
/// @return a std::string
inline std::string i2s(uint8_t val) { return std::bitset<8>(val).to_string(); }
/// Convert std::string to C string
#define I2CS(val) (i2s(val).c_str())

///////////////////////////////////////////////////////////////////////////////
// The WK2132Register methods
///////////////////////////////////////////////////////////////////////////////
uint8_t WK2132RegisterSPI::get() const {
  uint8_t value = 0x00;
  WK2132ComponentSPI *rcp = static_cast<WK2132ComponentSPI *>(this->parent_);
  auto command = command_byte(READ, this->register_, this->channel_, 0);
  rcp->enable();
  rcp->write_byte(command);
  rcp->read_array(&value, 1);
  rcp->disable();
  ESP_LOGVV(TAG, "Register::get() cmd=%s reg=%s ch=%d buf=%02X", I2CS(command),
            reg_to_str(this->register_, rcp->page1_), this->channel_, value);
  return value;
}

void WK2132RegisterSPI::read_fifo(uint8_t *data, size_t length) const {
  WK2132ComponentSPI *rcp = static_cast<WK2132ComponentSPI *>(this->parent_);
  auto command = command_byte(READ, this->register_, this->channel_, 1);
  rcp->enable();
  rcp->write_byte(command);
  rcp->read_array(data, length);
  rcp->disable();
  ESP_LOGVV(TAG, "Register::read_fifo() cmd=%s reg=%s ch=%d buf=%s", I2CS(command),
            reg_to_str(this->register_, rcp->page1_), this->channel_, format_hex_pretty(data, length).c_str());
}

void WK2132RegisterSPI::set(uint8_t value) {
  WK2132ComponentSPI *rcp = static_cast<WK2132ComponentSPI *>(this->parent_);
  auto command = command_byte(WRITE, this->register_, this->channel_, 1);
  rcp->enable();
  rcp->write_byte(command);
  rcp->write_array(&value, 1);
  rcp->disable();
  ESP_LOGVV(TAG, "Register::set() cmd=%s reg=%s ch=%d buf=%02X", I2CS(command),
            reg_to_str(this->register_, rcp->page1_), this->channel_, value);
}

void WK2132RegisterSPI::write_fifo(const uint8_t *data, size_t length) {
  WK2132ComponentSPI *rcp = static_cast<WK2132ComponentSPI *>(this->parent_);
  auto command = command_byte(READ, this->register_, this->channel_, 1);
  rcp->enable();
  rcp->write_byte(command);
  rcp->write_array(data, length);
  rcp->disable();
  ESP_LOGVV(TAG, "Register::write_fifo() cmd=%s reg=%s ch=%d buf=%s", I2CS(command),
            reg_to_str(this->register_, rcp->page1_), this->channel_, format_hex_pretty(data, length).c_str());
}

///////////////////////////////////////////////////////////////////////////////
// The WK2132Component methods
///////////////////////////////////////////////////////////////////////////////
void WK2132ComponentSPI::setup() {
  using namespace wk2132;
  ESP_LOGCONFIG(TAG, "Setting up wk2132_spi: %s with %d UARTs at @%02X ...", this->get_name(), this->children_.size(),
                this->base_address_);

  // enable both channels
  *this->global_reg_ptr(REG_WK2132_GENA) = GENA_C1EN | GENA_C2EN;
  // reset channels
  *this->global_reg_ptr(REG_WK2132_GRST) = GRST_C1RST | GRST_C2RST;
  // initialize the spage register to page 0
  *this->global_reg_ptr(REG_WK2132_SPAGE) = 0;
  this->page1_ = false;

  // we setup our children channels
  for (auto *child : this->children_) {
    child->setup_channel_();
  }
}

void WK2132ComponentSPI::dump_config() {
  ESP_LOGCONFIG(TAG, "Initialization of %s with %d UARTs completed", this->get_name(), this->children_.size());
  ESP_LOGCONFIG(TAG, "  Crystal: %d", this->crystal_);
  if (test_mode_)
    ESP_LOGCONFIG(TAG, "  Test mode: %d", test_mode_);
  LOG_PIN("  CS Pin: ", this->cs_);

  for (auto *child : this->children_) {
    child->dump_channel_();
  }
}

}  // namespace wk2132_spi
}  // namespace esphome
