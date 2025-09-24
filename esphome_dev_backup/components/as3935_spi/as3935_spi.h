#pragma once

#include "esphome/core/component.h"
#include "esphome/components/as3935/as3935.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace as3935_spi {

enum AS3935RegisterMasks { SPI_READ_M = 0x40 };

class SPIAS3935Component : public as3935::AS3935Component,
                           public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                 spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_2MHZ> {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  void write_register(uint8_t reg, uint8_t mask, uint8_t bits, uint8_t start_position) override;
  uint8_t read_register(uint8_t reg) override;
};

}  // namespace as3935_spi
}  // namespace esphome
