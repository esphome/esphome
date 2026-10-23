#pragma once

#ifdef USE_ZEPHYR

#include "spi.h"
#include <zephyr/drivers/spi.h>

namespace esphome::spi {

class ZephyrSPIDelegate;

class ZephyrSPIBus : public SPIBus {
  friend class ZephyrSPIDelegate;

 public:
  explicit ZephyrSPIBus(const device *dev) : dev_(dev) {
    this->idle_cfg_.frequency = IDLE_FREQUENCY;
    this->idle_cfg_.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8);
  }

  bool is_hw() override { return true; }

  SPIDelegate *get_delegate(uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin,
                            bool release_device, bool write_only) override;

 protected:
  // The lowest rate the SPIM driver accepts; only ever applied with no device selected
  static constexpr uint32_t IDLE_FREQUENCY = 125000;

  const device *dev_;
  // Delegate whose mode and frequency the driver last applied
  const ZephyrSPIDelegate *active_{nullptr};
  // Applied when the active delegate is deleted, so the driver never keeps a pointer to a freed config
  spi_config idle_cfg_{};
};

class ZephyrSPIDelegate : public SPIDelegate {
 public:
  ZephyrSPIDelegate(ZephyrSPIBus *bus, uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin);
  ~ZephyrSPIDelegate() override;

  void begin_transaction() override;

  uint8_t transfer(uint8_t data) override;
  void transfer(uint8_t *ptr, size_t length) override;
  void transfer(const uint8_t *txbuf, uint8_t *rxbuf, size_t length) override;
  void write_array(const uint8_t *ptr, size_t length) override;
  void write_array16(const uint16_t *data, size_t length) override;
  void read_array(uint8_t *ptr, size_t length) override;
  void write_cmd_addr_data(size_t cmd_bits, uint32_t cmd, size_t addr_bits, uint32_t address, const uint8_t *data,
                           size_t length, uint8_t bus_width) override;

 protected:
  /// One spi_transceive; rx (if given) is zeroed when the transfer fails
  void transceive_(const uint8_t *tx, uint8_t *rx, size_t length);
  // WORKAROUND: Zephyr's ESP32 SPIM driver sends cmd/addr on one line only, so one
  // spi_transceive() cannot do 1-line cmd+addr then N-line data. Sends them as two
  // transfers with CS (driven by ESPHome) held low. Remove once the driver supports it
  // (still missing in zephyr main on 2026-08-28).
  void write_cmd_addr_data_split_workaround_(size_t cmd_bits, uint32_t cmd, size_t addr_bits, uint32_t address,
                                             const uint8_t *data, size_t length, uint8_t bus_width);

  ZephyrSPIBus *bus_;
  // CS is driven by ESPHome via SPIDelegate::cs_pin_; cfg_.cs stays unset so
  // the Zephyr driver never touches a devicetree cs-gpios pin itself.
  spi_config cfg_{};
  // The driver caches configs by address, so each line mode needs its own long-lived config
  spi_config quad_cfg_{};
  spi_config octal_cfg_{};
};

}  // namespace esphome::spi

#endif  // USE_ZEPHYR
