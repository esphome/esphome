#pragma once

#include "esphome/core/component.h"
#include "esphome/components/pn532/pn532.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace pn532_spi {

class PN532Spi : public pn532::PN532,
                 public spi::SPIDevice<spi::BIT_ORDER_LSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                       spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;

  void dump_config() override;

 protected:
  void pn532_write_command_(const std::vector<uint8_t> &data) override;

  std::vector<uint8_t> pn532_read_data_() override;

  bool is_ready() override;

  bool read_ack_() override;

  std::vector<uint8_t> pn532_read_bytes(uint8_t len) override {
    std::vector<uint8_t> data;
    this->read_array(data.data(), len);
    return data;
  };

  void pn532_write_bytes(std::vector<uint8_t> data) override {
    this->write_array(data);
  };
};

}  // namespace pn532_spi
}  // namespace esphome
