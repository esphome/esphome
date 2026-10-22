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

}  // namespace esphome::spi

#endif  // USE_ZEPHYR
