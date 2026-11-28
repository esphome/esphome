#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"

#include <cinttypes>

namespace esphome::max31855 {

class MAX31855Sensor final : public sensor::Sensor,
                             public PollingComponent,
                             // SPI mode 0: SCK idles low, D31 is valid when CS falls and SO changes on
                             // the falling SCK edge (datasheet Figures 1 and 2).
                             public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                   spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
  SUB_SENSOR(reference)

 public:
  void setup() override;
  void dump_config() override;

  void update() override;

 protected:
  void read_data_();
};

}  // namespace esphome::max31855
