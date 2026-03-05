#pragma once

#include "esphome/core/component.h"
#include "esphome/components/nfc/nci_core.h"
#include "esphome/components/pn7160/pn7160.h"
#include "esphome/components/spi/spi.h"

#include <vector>

namespace esphome {
namespace pn7160_spi {

static const uint8_t TDD_SPI_READ = 0xFF;
static const uint8_t TDD_SPI_WRITE = 0x0A;

class PN7160Spi : public pn7160::PN7160,
                  public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                        spi::DATA_RATE_4MHZ> {
 public:
  void setup() override;

  void dump_config() override;

 protected:
  uint8_t read_nfcc(nfc::NciMessage &rx, uint16_t timeout) override;
  uint8_t write_nfcc(nfc::NciMessage &tx) override;
};

}  // namespace pn7160_spi
}  // namespace esphome
