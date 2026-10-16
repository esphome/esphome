/// @file weikai_spi.cpp
/// @brief  WeiKai component family - classes implementation
/// @date Last Modified: 2024/04/06 14:46:09
/// @details The classes declared in this file can be used by the Weikai family

#include "weikai_spi.h"
#include <cstring>

namespace esphome::weikai_spi {
using namespace weikai;
ESPHOME_LOG_TAG(TAG, "weikai_spi");

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
  // one single full duplex transfer: the command goes out in the first byte, the register value comes back in the
  // second one (saves one call to the SPI driver compared to write_byte() + read_byte())
  uint8_t buf[2]{cmd_byte(REG, READ_CMD, this->channel_, this->register_), 0x00};
  [[maybe_unused]] uint8_t const cmd = buf[0];
  spi_comp->enable();
  spi_comp->transfer_array(buf, 2);
  spi_comp->disable();
  uint8_t const val = buf[1];
  char bin_buf[9];
  ESP_LOGVV(TAG, "WeikaiRegisterSPI::read_reg() cmd=%s(%02X) reg=%s ch=%d buf=%02X", format_bin_to(bin_buf, cmd), cmd,
            reg_to_str(this->register_, this->comp_->page1()), this->channel_, val);
  return val;
}

void WeikaiRegisterSPI::read_fifo(uint8_t *data, size_t length) const {
  auto *spi_comp = static_cast<WeikaiComponentSPI *>(this->comp_);
  uint8_t cmd = cmd_byte(FIFO, READ_CMD, this->channel_, this->register_);
  // one single driver call: the command goes out in the first byte, the FIFO data comes back in the following ones
  uint8_t buf[XFER_MAX_SIZE + 1];
  buf[0] = cmd;
  memset(buf + 1, 0, length);
  spi_comp->enable();
  spi_comp->transfer_array(buf, length + 1);
  spi_comp->disable();
  memcpy(data, buf + 1, length);
#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  char bin_buf[9];
  ESP_LOGVV(TAG, "WeikaiRegisterSPI::read_fifo() cmd=%s(%02X) ch=%d len=%d buffer", format_bin_to(bin_buf, cmd), cmd,
            this->channel_, length);
  print_buffer(data, length);
#endif
}

void WeikaiRegisterSPI::write_reg(uint8_t value) {
  auto *spi_comp = static_cast<WeikaiComponentSPI *>(this->comp_);
  uint8_t buf[2]{cmd_byte(REG, WRITE_CMD, this->channel_, this->register_), value};
  spi_comp->enable();
  spi_comp->write_array(buf, 2);
  spi_comp->disable();
  char bin_buf[9];
  ESP_LOGVV(TAG, "WeikaiRegisterSPI::write_reg() cmd=%s(%02X) reg=%s ch=%d buf=%02X", format_bin_to(bin_buf, buf[0]),
            buf[0], reg_to_str(this->register_, this->comp_->page1()), this->channel_, buf[1]);
}

void WeikaiRegisterSPI::write_fifo(uint8_t *data, size_t length) {
  auto *spi_comp = static_cast<WeikaiComponentSPI *>(this->comp_);
  uint8_t cmd = cmd_byte(FIFO, WRITE_CMD, this->channel_, this->register_);
  // one single driver call: command byte followed by the data
  uint8_t buf[XFER_MAX_SIZE + 1];
  buf[0] = cmd;
  memcpy(buf + 1, data, length);
  spi_comp->enable();
  spi_comp->write_array(buf, length + 1);
  spi_comp->disable();

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  char bin_buf[9];
  ESP_LOGVV(TAG, "WeikaiRegisterSPI::write_fifo() cmd=%s(%02X) ch=%d len=%d buffer", format_bin_to(bin_buf, cmd), cmd,
            this->channel_, length);
  print_buffer(data, length);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// The WeikaiComponentSPI methods
///////////////////////////////////////////////////////////////////////////////
void WeikaiComponentSPI::setup() {
  using namespace weikai;
  ESP_LOGCONFIG(TAG, "Setup %s (%d UARTs)", this->get_name(), this->children_.size());
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
  LOG_PIN("  CS Pin: ", this->cs_);

  for (auto *child : this->children_) {
    child->dump_channel();
  }
}

}  // namespace esphome::weikai_spi
