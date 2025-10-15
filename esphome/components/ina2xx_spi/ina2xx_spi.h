#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ina2xx_base/ina2xx_base.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace ina2xx_spi {

class INA2XXSPI : public ina2xx_base::INA2XX,
                  public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_TRAILING,
                                        spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  bool read_ina_register(uint8_t reg, uint8_t *data, size_t len) override;
  bool write_ina_register(uint8_t reg, const uint8_t *data, size_t len) override;
};
}  // namespace ina2xx_spi
}  // namespace esphome
