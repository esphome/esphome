#ifdef USE_ZEPHYR

#include "spi_zephyr.h"
#include "esphome/core/log.h"

namespace esphome::spi {

static const char *const TAG = "spi.zephyr";

static spi_operation_t build_operation(SPIBitOrder bit_order, SPIMode mode) {
  spi_operation_t op = SPI_OP_MODE_MASTER | SPI_WORD_SET(8);
  op |= (bit_order == BIT_ORDER_LSB_FIRST) ? SPI_TRANSFER_LSB : SPI_TRANSFER_MSB;
  if (Utility::get_polarity(mode) == CLOCK_POLARITY_HIGH)
    op |= SPI_MODE_CPOL;
  if (Utility::get_phase(mode) == CLOCK_PHASE_TRAILING)
    op |= SPI_MODE_CPHA;
  return op;
}

SPIDelegate *ZephyrSPIBus::get_delegate(uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin,
                                        bool release_device, bool write_only) {
  return new ZephyrSPIDelegate(this->dev_, data_rate, bit_order, mode, cs_pin);  // NOLINT
}

ZephyrSPIDelegate::ZephyrSPIDelegate(const device *dev, uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode,
                                     GPIOPin *cs_pin)
    : SPIDelegate(data_rate, bit_order, mode, cs_pin), dev_(dev) {
  this->cfg_.frequency = data_rate;
  this->cfg_.operation = build_operation(bit_order, mode);
  // CS is driven by ESPHome via SPIDelegate base class; no Zephyr-level CS control.
  this->cfg_.slave = 0;
}

void ZephyrSPIDelegate::do_transceive_(const uint8_t *tx, uint8_t *rx, size_t length) {
  if (length == 0)
    return;

  spi_buf tx_buf{};
  spi_buf rx_buf{};
  spi_buf_set tx_set{};
  spi_buf_set rx_set{};

  if (tx != nullptr) {
    tx_buf.buf = const_cast<uint8_t *>(tx);
    tx_buf.len = length;
    tx_set.buffers = &tx_buf;
    tx_set.count = 1;
  }
  if (rx != nullptr) {
    rx_buf.buf = rx;
    rx_buf.len = length;
    rx_set.buffers = &rx_buf;
    rx_set.count = 1;
  }

  int err = spi_transceive(this->dev_, &this->cfg_, tx != nullptr ? &tx_set : nullptr,
                           rx != nullptr ? &rx_set : nullptr);
  if (err != 0) {
    ESP_LOGE(TAG, "spi_transceive failed: %d", err);
  }
}

uint8_t ZephyrSPIDelegate::transfer(uint8_t data) {
  uint8_t rx = 0;
  this->do_transceive_(&data, &rx, 1);
  return rx;
}

void ZephyrSPIDelegate::transfer(uint8_t *ptr, size_t length) { this->do_transceive_(ptr, ptr, length); }

void ZephyrSPIDelegate::transfer(const uint8_t *txbuf, uint8_t *rxbuf, size_t length) {
  this->do_transceive_(txbuf, rxbuf, length);
}

void ZephyrSPIDelegate::write_array(const uint8_t *ptr, size_t length) { this->do_transceive_(ptr, nullptr, length); }

void ZephyrSPIDelegate::read_array(uint8_t *ptr, size_t length) { this->do_transceive_(nullptr, ptr, length); }

}  // namespace esphome::spi

#endif  // USE_ZEPHYR
