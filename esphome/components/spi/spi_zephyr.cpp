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
}

// NOTE: some Zephyr SPI drivers' DMA engines cannot read directly from flash-resident
// buffers (confirmed on nRF52's EasyDMA -- see upstream esphome/esphome#18214). Not
// yet verified one way or the other for STM32's DMA; if a display/flash-command
// buffer defined `static const` gets corrupted mid-transfer, stage it through a RAM
// buffer here the way that PR does for nRF52.
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

  int err =
      spi_transceive(this->dev_, &this->cfg_, tx != nullptr ? &tx_set : nullptr, rx != nullptr ? &rx_set : nullptr);
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
    spi_config header_cfg = this->cfg_;
    header_cfg.operation &= ~SPI_LINES_MASK;  // SPI_LINES_SINGLE
    spi_buf header_buf{header, header_len};
    spi_buf_set header_set{&header_buf, 1};
    int err = spi_transceive(this->dev_, &header_cfg, &header_set, nullptr);
    if (err != 0) {
      ESP_LOGE(TAG, "spi_transceive (cmd/addr) failed: %d", err);
      return;
    }
  }

  if (length == 0 || data == nullptr)
    return;

  spi_config data_cfg = this->cfg_;
  data_cfg.operation &= ~SPI_LINES_MASK;
  if (bus_width == 4) {
    data_cfg.operation |= SPI_LINES_QUAD;
  } else if (bus_width == 8) {
    data_cfg.operation |= SPI_LINES_OCTAL;
  }
  spi_buf data_buf{const_cast<uint8_t *>(data), length};
  spi_buf_set data_set{&data_buf, 1};
  int err = spi_transceive(this->dev_, &data_cfg, &data_set, nullptr);
  if (err != 0) {
    ESP_LOGE(TAG, "spi_transceive (data) failed: %d", err);
  }
}

}  // namespace esphome::spi

#endif  // USE_ZEPHYR
