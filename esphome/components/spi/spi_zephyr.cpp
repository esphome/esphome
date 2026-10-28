#ifdef USE_ZEPHYR

#include "spi_zephyr.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>

namespace esphome::spi {

ESPHOME_LOG_TAG(TAG, "spi.zephyr");

// Words swapped per transceive for MSB-first 16-bit writes
static constexpr size_t SWAP_BUFFER_WORDS = 64;

static spi_operation_t build_operation(SPIBitOrder bit_order, SPIMode mode) {
  spi_operation_t op = SPI_OP_MODE_MASTER | SPI_WORD_SET(8);
  op |= (bit_order == BIT_ORDER_LSB_FIRST) ? SPI_TRANSFER_LSB : SPI_TRANSFER_MSB;
  if (Utility::get_polarity(mode) == CLOCK_POLARITY_HIGH)
    op |= SPI_MODE_CPOL;
  if (Utility::get_phase(mode) == CLOCK_PHASE_TRAILING)
    op |= SPI_MODE_CPHA;
  return op;
}

// release_device and write_only are not needed here: the Zephyr driver locks the bus per transfer,
// and a null rx buffer already skips the receive DMA
SPIDelegate *ZephyrSPIBus::get_delegate(uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin,
                                        bool release_device, bool write_only) {
  return new ZephyrSPIDelegate(this, data_rate, bit_order, mode, cs_pin);  // NOLINT
}

ZephyrSPIDelegate::ZephyrSPIDelegate(ZephyrSPIBus *bus, uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode,
                                     GPIOPin *cs_pin)
    : SPIDelegate(data_rate, bit_order, mode, cs_pin), bus_(bus) {
  this->cfg_.frequency = data_rate;
  this->cfg_.operation = build_operation(bit_order, mode);
  this->quad_cfg_ = this->cfg_;
  this->quad_cfg_.operation |= SPI_LINES_QUAD;
  this->octal_cfg_ = this->cfg_;
  this->octal_cfg_.operation |= SPI_LINES_OCTAL;
}

// The driver skips a config at the address it applied last, so a delegate created later in this allocation
// would run with the old settings: hand it the bus's own config before this one goes away.
ZephyrSPIDelegate::~ZephyrSPIDelegate() {
  if (this->bus_->active_ != this)
    return;
  this->bus_->active_ = nullptr;
  spi_transceive(this->bus_->dev_, &this->bus_->idle_cfg_, nullptr, nullptr);
}

// The driver applies cfg_ (and the SCK idle level for CPOL) at the start of each transfer, so switch
// it over with an empty transfer before CS goes low; otherwise SCK changes level while CS is active.
void ZephyrSPIDelegate::begin_transaction() {
  if (this->bus_->active_ != this) {
    const int err = spi_transceive(this->bus_->dev_, &this->cfg_, nullptr, nullptr);
    if (err == 0) {
      this->bus_->active_ = this;
    } else {
      ESP_LOGE(TAG, "spi_transceive failed: %d", err);
    }
  }
  SPIDelegate::begin_transaction();
}

// Buffers EasyDMA cannot reach (flash tables, for one) are copied by the nrfx SPIM driver itself,
// CONFIG_SPI_NRFX_RAM_BUFFER_SIZE bytes at a time
void ZephyrSPIDelegate::transceive_(const uint8_t *tx, uint8_t *rx, size_t length) {
  if (length == 0)
    return;
  const spi_buf tx_buf{const_cast<uint8_t *>(tx), length};  // NOLINT(cppcoreguidelines-pro-type-const-cast)
  const spi_buf rx_buf{rx, length};
  const spi_buf_set tx_set{&tx_buf, 1};
  const spi_buf_set rx_set{&rx_buf, 1};
  const int err = spi_transceive(this->bus_->dev_, &this->cfg_, tx != nullptr ? &tx_set : nullptr,
                                 rx != nullptr ? &rx_set : nullptr);
  if (err != 0) {
    ESP_LOGE(TAG, "spi_transceive failed: %d", err);
    if (rx != nullptr)
      memset(rx, 0, length);
  }
}

uint8_t ZephyrSPIDelegate::transfer(uint8_t data) {
  uint8_t rx = 0;
  this->transceive_(&data, &rx, 1);
  return rx;
}

void ZephyrSPIDelegate::transfer(uint8_t *ptr, size_t length) { this->transceive_(ptr, ptr, length); }

void ZephyrSPIDelegate::transfer(const uint8_t *txbuf, uint8_t *rxbuf, size_t length) {
  this->transceive_(txbuf, rxbuf, length);
}

void ZephyrSPIDelegate::write_array(const uint8_t *ptr, size_t length) { this->transceive_(ptr, nullptr, length); }

void ZephyrSPIDelegate::write_array16(const uint16_t *data, size_t length) {
  if (this->bit_order_ == BIT_ORDER_LSB_FIRST) {
    this->write_array(reinterpret_cast<const uint8_t *>(data), length * 2);
    return;
  }
  uint16_t buffer[SWAP_BUFFER_WORDS];
  while (length != 0) {
    const size_t partial = std::min(length, SWAP_BUFFER_WORDS);
    for (size_t i = 0; i != partial; i++)
      buffer[i] = byteswap(data[i]);
    this->write_array(reinterpret_cast<const uint8_t *>(buffer), partial * 2);
    data += partial;
    length -= partial;
  }
}

void ZephyrSPIDelegate::read_array(uint8_t *ptr, size_t length) { this->transceive_(nullptr, ptr, length); }

void ZephyrSPIDelegate::write_cmd_addr_data(size_t cmd_bits, uint32_t cmd, size_t addr_bits, uint32_t address,
                                            const uint8_t *data, size_t length, uint8_t bus_width) {
  this->write_cmd_addr_data_split_workaround_(cmd_bits, cmd, addr_bits, address, data, length, bus_width);
}

void ZephyrSPIDelegate::write_cmd_addr_data_split_workaround_(size_t cmd_bits, uint32_t cmd, size_t addr_bits,
                                                              uint32_t address, const uint8_t *data, size_t length,
                                                              uint8_t bus_width) {
  if (cmd_bits % 8 != 0 || addr_bits % 8 != 0) {
    ESP_LOGE(TAG, "write_cmd_addr_data: cmd_bits/addr_bits must be byte-aligned on this backend");
    return;
  }

  uint8_t header[8];
  size_t header_len = 0;
  for (size_t i = cmd_bits / 8; i > 0; i--)
    header[header_len++] = (cmd >> ((i - 1) * 8)) & 0xFF;
  for (size_t i = addr_bits / 8; i > 0; i--)
    header[header_len++] = (address >> ((i - 1) * 8)) & 0xFF;

  if (header_len != 0) {
    spi_buf header_buf{header, header_len};
    spi_buf_set header_set{&header_buf, 1};
    int err = spi_transceive(this->bus_->dev_, &this->cfg_, &header_set, nullptr);
    if (err != 0) {
      ESP_LOGE(TAG, "spi_transceive (cmd/addr) failed: %d", err);
      return;
    }
  }

  if (length == 0 || data == nullptr)
    return;

  const spi_config *data_cfg = bus_width == 4 ? &this->quad_cfg_ : bus_width == 8 ? &this->octal_cfg_ : &this->cfg_;
  spi_buf data_buf{const_cast<uint8_t *>(data), length};  // NOLINT(cppcoreguidelines-pro-type-const-cast)
  spi_buf_set data_set{&data_buf, 1};
  int err = spi_transceive(this->bus_->dev_, data_cfg, &data_set, nullptr);
  if (err != 0) {
    ESP_LOGE(TAG, "spi_transceive (data) failed: %d", err);
  }
}

}  // namespace esphome::spi

#endif  // USE_ZEPHYR
