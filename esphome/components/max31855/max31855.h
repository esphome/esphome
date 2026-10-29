#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"

#include <cinttypes>

namespace esphome::max31855 {

class MAX31855Sensor final : public sensor::Sensor,
                             public PollingComponent,
                             public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH,
                                                   spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_4MHZ> {
  SUB_SENSOR(reference)

 public:
  void setup() override;
  void dump_config() override;

  void update() override;

 protected:
  void read_data_();
};

}  // namespace esphome::max31855
