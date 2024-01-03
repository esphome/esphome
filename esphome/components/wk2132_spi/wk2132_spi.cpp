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

enum CmdType { WRITE_CMD = 0, READ_CMD = 1 };  ///< Read or Write transfer
enum RegType { REG = 0, FIFO = 1 };            ///< Register or FIFO

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
/// FIFO: 0 = register, 1 = FIFO
/// R/W: 0 = write, 1 = read
/// C1-C0: Channel (0-1)
/// A3-A0: Address (0-F)
inline static uint8_t cmd_byte(CmdType transfer_type, RegType fifo, uint8_t reg, uint8_t channel) {
  return (fifo << 7 | transfer_type << 6 | channel << 4 | reg << 0);
}

/// @brief convert an int to binary representation as C++ std::string
/// @param val integer to convert
/// @return a std::string
inline std::string i2s(uint8_t val) { return std::bitset<8>(val).to_string(); }
/// Convert std::string to C string
#define I2CS(val) (i2s(val).c_str())

///////////////////////////////////////////////////////////////////////////////
// The WK2132Reg methods
///////////////////////////////////////////////////////////////////////////////
uint8_t WK2132RegisterSPI::read_reg() const {
  auto *spi_delegate = static_cast<WK2132ComponentSPI *>(this->comp_)->delegate_;
  uint8_t buf[2]{cmd_byte(READ_CMD, REG, this->register_, this->channel_)};
  spi_delegate->begin_transaction();
  spi_delegate->transfer(buf, 2);
  spi_delegate->end_transaction();
  ESP_LOGVV(TAG, "Register::read_reg() cmd=%s(%02X) reg=%s(%02X) ch=%d buf=%02X", I2CS(buf[0]), buf[0],
            reg_to_str(this->register_, this->comp_->page1_), this->register_, this->channel_, buf[1]);
  return buf[1];
}

void WK2132RegisterSPI::read_fifo(uint8_t *data, size_t length) const {
  auto *spi_delegate = static_cast<WK2132ComponentSPI *>(this->comp_)->delegate_;
  uint8_t cmd = cmd_byte(READ_CMD, FIFO, this->register_, this->channel_);
  spi_delegate->begin_transaction();
  spi_delegate->transfer(&cmd, 1);
  spi_delegate->transfer(data, length);
  spi_delegate->end_transaction();
  ESP_LOGVV(TAG, "Register::read_fifo() cmd=%s(%02X) ch=%d buf=%hhu", I2CS(cmd), cmd, this->register_, this->channel_,
            format_hex_pretty(data, length).c_str());
}

void WK2132RegisterSPI::write_reg(uint8_t value) {
  auto *spi_delegate = static_cast<WK2132ComponentSPI *>(this->comp_)->delegate_;
  uint8_t buf[2]{cmd_byte(WRITE_CMD, REG, this->register_, this->channel_), value};
  spi_delegate->begin_transaction();
  spi_delegate->transfer(buf, 2);
  spi_delegate->end_transaction();
  ESP_LOGVV(TAG, "Register::write_reg() cmd=%s(%02X) reg=%s(%02X) ch=%d buf=%02X", I2CS(buf[0]), buf[0],
            reg_to_str(this->register_, this->comp_->page1_), this->register_, this->channel_, value);
}

void WK2132RegisterSPI::write_fifo(uint8_t *data, size_t length) {
  auto *spi_delegate = static_cast<WK2132ComponentSPI *>(this->comp_)->delegate_;
  uint8_t cmd = cmd_byte(WRITE_CMD, FIFO, this->register_, this->channel_);
  spi_delegate->begin_transaction();
  spi_delegate->transfer(&cmd, 1);
  spi_delegate->transfer(data, length);
  spi_delegate->end_transaction();
  ESP_LOGVV(TAG, "Register::read_fifo() cmd=%s(%02X) ch=%d buf=%hhu", I2CS(cmd), cmd, this->register_, this->channel_,
            format_hex_pretty(data, length).c_str());
}

///////////////////////////////////////////////////////////////////////////////
// The WK2132Component methods
///////////////////////////////////////////////////////////////////////////////
void WK2132ComponentSPI::setup() {
  using namespace wk2132;
  ESP_LOGCONFIG(TAG, "Setting up wk2132_spi: %s with %d UARTs...", this->get_name(), this->children_.size());
  this->spi_setup();
  // enable both channels
  this->reg(REG_WK2132_GENA) = GENA_C1EN | GENA_C2EN;
  // reset channels
  this->reg(REG_WK2132_GRST) = GRST_C1RST | GRST_C2RST;
  // initialize the spage register to page 0
  this->reg(REG_WK2132_SPAGE) = 0;
  this->page1_ = false;

  // we setup our children channels
  for (auto *child : this->children_) {
    child->setup_channel();
  }
}

void WK2132ComponentSPI::dump_config() {
  ESP_LOGCONFIG(TAG, "Initialization of %s with %d UARTs completed", this->get_name(), this->children_.size());
  ESP_LOGCONFIG(TAG, "  Crystal: %d", this->crystal_);
  if (test_mode_)
    ESP_LOGCONFIG(TAG, "  Test mode: %d", test_mode_);
  LOG_PIN("  CS Pin: ", this->cs_);

  for (auto *child : this->children_) {
    child->dump_channel();
  }
}

}  // namespace wk2132_spi
}  // namespace esphome
