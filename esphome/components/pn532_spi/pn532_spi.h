#pragma once

#include "esphome/core/component.h"
#include "esphome/components/pn532/pn532.h"
#include "esphome/components/spi/spi.h"

#include <span>

namespace esphome::pn532_spi {

class PN532Spi final : public pn532::PN532,
                       public spi::SPIDevice<spi::BIT_ORDER_LSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                             spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;

  void dump_config() override;

 protected:
  bool is_read_ready() override;
  bool write_data(std::span<const uint8_t> data) override;
  bool read_data(pn532::PN532Frame &data, size_t len) override;
  bool read_response(uint8_t command, pn532::PN532Frame &data) override;
};

}  // namespace esphome::pn532_spi
