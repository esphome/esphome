#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"

namespace esphome::max6675 {

class MAX6675Sensor : public sensor::Sensor,
                      public PollingComponent,
                      public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                            spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void dump_config() override;

  void update() override;

 protected:
  void read_data_();
};

}  // namespace esphome::max6675
