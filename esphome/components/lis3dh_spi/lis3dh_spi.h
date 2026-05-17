#pragma once

#include "esphome/components/lis3dh/lis3dh.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace lis3dh_spi {

class LIS3DHSPI : public lis3dh::LIS3DHComponent,
                  public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                        spi::DATA_RATE_2MHZ> {
 public:
  void setup() override {
    this->spi_setup();
    LIS3DHComponent::setup();
  }
  void dump_config() override;

  bool read_register(uint8_t reg, uint8_t *data, uint16_t len) override;
  bool write_register(uint8_t reg, const uint8_t *data, uint16_t len) override;
};

}  // namespace lis3dh_spi
}  // namespace esphome
