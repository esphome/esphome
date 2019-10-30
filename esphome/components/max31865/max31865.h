#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace max31865 {

class MAX31865Sensor : public sensor::Sensor,
                       public PollingComponent,
                       public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                             spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_4MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_pin_count(bool even) { this->even_pins_ = even; }
  void set_r_nominal(float r = 1000) { this->r_nominal_ = r; };
  void set_r_ref(float r = 4300) { this->r_ref_ = r; };
  void set_filter_50hz(bool b = false) { this->filter_50hz_ = b; };

  void update() override;

  float temperature(unsigned short adc, float rtd_nominal, float ref_resistor);

 protected:
  void read_data_();
  void write_config_();
  bool even_pins_;
  float r_nominal_;
  float r_ref_;
  bool filter_50hz_;
};

}  // namespace max31865
}  // namespace esphome
