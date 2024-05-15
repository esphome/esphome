/// @file weikai_spi.cpp
/// @brief  WeiKai component family - classes implementation
/// @date Last Modified: 2024/04/06 14:46:09
/// @details The classes declared in this file can be used by the Weikai family

#include "weikai_spi.h"

namespace esphome {
namespace weikai_spi {
using namespace weikai;
static const char *const TAG = "weikai_spi";

/// @brief convert an int to binary representation as C++ std::string
/// @param val integer to convert
/// @return a std::string
inline std::string i2s(uint8_t val) { return std::bitset<8>(val).to_string(); }
/// Convert std::string to C string
#define I2S2CS(val) (i2s(val).c_str())

/// @brief measure the time elapsed between two calls
/// @param last_time time of the previous call
/// @return the elapsed time in microseconds
uint32_t elapsed_ms(uint32_t &last_time) {
  uint32_t e = millis() - last_time;
  last_time = millis();
  return e;
};

/// @brief Converts the parity enum value to a C string
/// @param parity enum
/// @return the string
const char *p2s(uart::UARTParityOptions parity) {
  using namespace uart;
  switch (parity) {
    case UART_CONFIG_PARITY_NONE:
      return "NONE";
    case UART_CONFIG_PARITY_EVEN:
      return "EVEN";
    case UART_CONFIG_PARITY_ODD:
      return "ODD";
    default:
      return "UNKNOWN";
  }
}

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

enum RegType { REG = 0, FIFO = 1 };            ///< Register or FIFO
enum CmdType { WRITE_CMD = 0, READ_CMD = 1 };  ///< Read or Write transfer

/// @brief Computes the SPI command byte
/// @param transfer_type read or write command
/// @param reg (0-15) the address of the register
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
inline static uint8_t cmd_byte(RegType fifo, CmdType transfer_type, uint8_t channel, uint8_t reg) {
  return (fifo << 7 | transfer_type << 6 | channel << 4 | reg << 0);
}

///////////////////////////////////////////////////////////////////////////////
// The WeikaiRegisterSPI methods
///////////////////////////////////////////////////////////////////////////////
uint8_t WeikaiRegisterSPI::read_reg() const {
  auto *spi_comp = static_cast<WeikaiComponentSPI *>(this->comp_);
  uint8_t cmd = cmd_byte(REG, READ_CMD, this->channel_, this->register_);
  spi_comp->enable();
  spi_comp->write_byte(cmd);
  uint8_t val = spi_comp->read_byte();
  spi_comp->disable();
  ESP_LOGVV(TAG, "WeikaiRegisterSPI::read_reg() cmd=%s(%02X) reg=%s ch=%d buf=%02X", I2S2CS(cmd), cmd,
            reg_to_str(this->register_, this->comp_->page1()), this->channel_, val);
  return val;
}

void WeikaiRegisterSPI::read_fifo(uint8_t *data, size_t length) const {
  auto *spi_comp = static_cast<WeikaiComponentSPI *>(this->comp_);
  uint8_t cmd = cmd_byte(FIFO, READ_CMD, this->channel_, this->register_);
  spi_comp->enable();
  spi_comp->write_byte(cmd);
  spi_comp->read_array(data, length);
  spi_comp->disable();
#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  ESP_LOGVV(TAG, "WeikaiRegisterSPI::read_fifo() cmd=%s(%02X) ch=%d len=%d buffer", I2S2CS(cmd), cmd, this->channel_,
            length);
  print_buffer(data, length);
#endif
}

void WeikaiRegisterSPI::write_reg(uint8_t value) {
  auto *spi_comp = static_cast<WeikaiComponentSPI *>(this->comp_);
  uint8_t buf[2]{cmd_byte(REG, WRITE_CMD, this->channel_, this->register_), value};
  spi_comp->enable();
  spi_comp->write_array(buf, 2);
  spi_comp->disable();
  ESP_LOGVV(TAG, "WeikaiRegisterSPI::write_reg() cmd=%s(%02X) reg=%s ch=%d buf=%02X", I2S2CS(buf[0]), buf[0],
            reg_to_str(this->register_, this->comp_->page1()), this->channel_, buf[1]);
}

void WeikaiRegisterSPI::write_fifo(uint8_t *data, size_t length) {
  auto *spi_comp = static_cast<WeikaiComponentSPI *>(this->comp_);
  uint8_t cmd = cmd_byte(FIFO, WRITE_CMD, this->channel_, this->register_);
  spi_comp->enable();
  spi_comp->write_byte(cmd);
  spi_comp->write_array(data, length);
  spi_comp->disable();

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  ESP_LOGVV(TAG, "WeikaiRegisterSPI::write_fifo() cmd=%s(%02X) ch=%d len=%d buffer", I2S2CS(cmd), cmd, this->channel_,
            length);
  print_buffer(data, length);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// The WeikaiComponentSPI methods
///////////////////////////////////////////////////////////////////////////////
void WeikaiComponentSPI::setup() {
  using namespace weikai;
  ESP_LOGCONFIG(TAG, "Setting up wk2168_spi: %s with %d UARTs...", this->get_name(), this->children_.size());
  this->spi_setup();
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

void WeikaiComponentSPI::dump_config() {
  ESP_LOGCONFIG(TAG, "Initialization of %s with %d UARTs completed", this->get_name(), this->children_.size());
  ESP_LOGCONFIG(TAG, "  Crystal: %" PRIu32 "", this->crystal_);
  if (test_mode_)
    ESP_LOGCONFIG(TAG, "  Test mode: %d", test_mode_);
  ESP_LOGCONFIG(TAG, "  Transfer buffer size: %d", XFER_MAX_SIZE);
  LOG_PIN("  CS Pin: ", this->cs_);

  for (auto *child : this->children_) {
    child->dump_channel();
  }
}

}  // namespace weikai_spi
}  // namespace esphome
