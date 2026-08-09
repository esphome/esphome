#ifdef USE_ZEPHYR

#include "spi_zephyr.h"
#include <cstdint>
#include <cstring>
#include "esphome/core/log.h"

namespace esphome::spi {

static const char *const TAG = "spi.zephyr";

// nRF52's SPIM peripheral uses EasyDMA, which can only access RAM -- flash reads silently fail
// (garbage or nothing is transferred, with no error reported). ESPHome epaper drivers commonly
// pass `static const` command tables, which live in flash. RAM starts at 0x20000000 on every
// nRF52 part, so anything below that needs to be staged through a RAM buffer first.
static bool is_ram_address(const void *ptr) { return reinterpret_cast<uintptr_t>(ptr) >= 0x20000000UL; }

static constexpr size_t FLASH_STAGING_SIZE = 64;

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
}

void ZephyrSPIDelegate::do_transceive_(const uint8_t *tx, uint8_t *rx, size_t length) {
  if (length == 0)
    return;

  if (tx != nullptr && !is_ram_address(tx)) {
    uint8_t chunk[FLASH_STAGING_SIZE];
    size_t remaining = length;
    const uint8_t *src = tx;
    uint8_t *dst_rx = rx;
    while (remaining > 0) {
      size_t n = remaining < FLASH_STAGING_SIZE ? remaining : FLASH_STAGING_SIZE;
      memcpy(chunk, src, n);
      if (!this->transceive_chunk_(chunk, dst_rx, n)) {
        // The rest of rx wasn't touched by this or any prior chunk; zero it too so the
        // whole buffer is deterministic rather than part-result, part-stale data.
        if (dst_rx != nullptr)
          memset(dst_rx + n, 0, remaining - n);
        return;
      }
      src += n;
      if (dst_rx != nullptr)
        dst_rx += n;
      remaining -= n;
    }
    return;
  }

  this->transceive_chunk_(tx, rx, length);
}

bool ZephyrSPIDelegate::transceive_chunk_(const uint8_t *tx, uint8_t *rx, size_t length) {
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

  int err =
      spi_transceive(this->dev_, &this->cfg_, tx != nullptr ? &tx_set : nullptr, rx != nullptr ? &rx_set : nullptr);
  if (err != 0) {
    ESP_LOGE(TAG, "spi_transceive failed: %d", err);
    if (rx != nullptr)
      memset(rx, 0, length);
    return false;
  }
  return true;
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

void ZephyrSPIDelegate::write_array16(const uint16_t *data, size_t length) {
  if (this->bit_order_ == BIT_ORDER_LSB_FIRST) {
    this->write_array(reinterpret_cast<const uint8_t *>(data), length * 2);
    return;
  }
  // MSB-first needs each 16-bit word byte-swapped before it goes out; stage the swap
  // through a small stack buffer instead of one write16() (and one transceive) per pixel.
  static constexpr size_t SWAP_BUFFER_WORDS = FLASH_STAGING_SIZE / 2;
  uint16_t buffer[SWAP_BUFFER_WORDS];
  while (length != 0) {
    size_t partial = length < SWAP_BUFFER_WORDS ? length : SWAP_BUFFER_WORDS;
    for (size_t i = 0; i != partial; i++)
      buffer[i] = (data[i] >> 8) | (data[i] << 8);
    this->write_array(reinterpret_cast<const uint8_t *>(buffer), partial * 2);
    data += partial;
    length -= partial;
  }
}

void ZephyrSPIDelegate::read_array(uint8_t *ptr, size_t length) { this->do_transceive_(nullptr, ptr, length); }

}  // namespace esphome::spi

#endif  // USE_ZEPHYR
