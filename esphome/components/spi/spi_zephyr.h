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
  void write_array16(const uint16_t *data, size_t length) override;
  void read_array(uint8_t *ptr, size_t length) override;

 protected:
  void do_transceive_(const uint8_t *tx, uint8_t *rx, size_t length);
  // Returns false if the transfer failed; rx (if given) is zeroed in that case.
  bool transceive_chunk_(const uint8_t *tx, uint8_t *rx, size_t length);

  const device *dev_;
  // CS is driven by ESPHome via SPIDelegate::cs_pin_; cfg_.cs stays unset so
  // the Zephyr driver never touches a devicetree cs-gpios pin itself.
  spi_config cfg_{};
};

}  // namespace esphome::spi

#endif  // USE_ZEPHYR
