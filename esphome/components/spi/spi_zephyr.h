#pragma once

#ifdef USE_ZEPHYR

#include "spi.h"
#include <zephyr/drivers/spi.h>

namespace esphome::spi {

class ZephyrSPIBus : public SPIBus {
 public:
  explicit ZephyrSPIBus(const device *dev) : dev_(dev) {}

  bool is_hw() override { return true; }

  SPIDelegate *get_delegate(uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin,
                            bool release_device, bool write_only) override;

 protected:
  const device *dev_;
};

class ZephyrSPIDelegate : public SPIDelegate {
 public:
  ZephyrSPIDelegate(const device *dev, uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin);

  uint8_t transfer(uint8_t data) override;
  void transfer(uint8_t *ptr, size_t length) override;
  void transfer(const uint8_t *txbuf, uint8_t *rxbuf, size_t length) override;
  void write_array(const uint8_t *ptr, size_t length) override;
  void read_array(uint8_t *ptr, size_t length) override;
  void write_cmd_addr_data(size_t cmd_bits, uint32_t cmd, size_t addr_bits, uint32_t address, const uint8_t *data,
                           size_t length, uint8_t bus_width) override;

 protected:
  void do_transceive_(const uint8_t *tx, uint8_t *rx, size_t length);
  // WORKAROUND for zephyrproject-rtos/zephyr drivers/spi/spi_esp32_spim.c:
  // spi_esp32_configure() hardcodes addr_lines=1/cmd_lines=1 ("multiline for command
  // and address not supported"), so a single spi_transceive() can't do the 1-line
  // cmd+addr / N-line data pattern real quad/octal displays and flash use. Splits it
  // into two transceive() calls with CS held low across both (CS is a plain
  // ESPHome-driven GPIO here, never touched by spi_transceive() itself -- see
  // SPIDelegate::begin_transaction()/end_transaction() in spi.h). Delete this method
  // (and call spi_transceive() directly from write_cmd_addr_data()) once that
  // limitation is fixed upstream -- checked against zephyr@main on 2026-08-28, still
  // present, no tracked upstream issue.
  void write_cmd_addr_data_split_workaround_(size_t cmd_bits, uint32_t cmd, size_t addr_bits, uint32_t address,
                                             const uint8_t *data, size_t length, uint8_t bus_width);

  const device *dev_;
  spi_config cfg_{};
};

}  // namespace esphome::spi

#endif  // USE_ZEPHYR
