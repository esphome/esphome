#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"

#include <cinttypes>

namespace esphome {
namespace max31855 {

class MAX31855Sensor : public sensor::Sensor,
                       public PollingComponent,
                       public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH,
                                             spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_4MHZ> {
 public:
  void set_reference_sensor(sensor::Sensor *temperature_sensor) { temperature_reference_ = temperature_sensor; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void update() override;

 protected:
  void read_data_();
  sensor::Sensor *temperature_reference_{nullptr};
};

}  // namespace max31855
}  // namespace esphome
