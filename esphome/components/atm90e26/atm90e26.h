#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace atm90e26 {

class ATM90E26Component : public PollingComponent,
                          public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH,
                                                spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_200KHZ> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_voltage_sensor(sensor::Sensor *obj) { this->voltage_sensor_ = obj; }
  void set_current_sensor(sensor::Sensor *obj) { this->current_sensor_ = obj; }
  void set_power_sensor(sensor::Sensor *obj) { this->power_sensor_ = obj; }
  void set_reactive_power_sensor(sensor::Sensor *obj) { this->reactive_power_sensor_ = obj; }
  void set_forward_active_energy_sensor(sensor::Sensor *obj) { this->forward_active_energy_sensor_ = obj; }
  void set_reverse_active_energy_sensor(sensor::Sensor *obj) { this->reverse_active_energy_sensor_ = obj; }
  void set_power_factor_sensor(sensor::Sensor *obj) { this->power_factor_sensor_ = obj; }
  void set_freq_sensor(sensor::Sensor *freq_sensor) { freq_sensor_ = freq_sensor; }
  void set_line_freq(int freq) { line_freq_ = freq; }
  void set_meter_constant(float val) { meter_constant_ = val; }
  void set_pl_const(uint32_t pl_const) { pl_const_ = pl_const; }
  void set_gain_metering(uint16_t gain) { this->gain_metering_ = gain; }
  void set_gain_voltage(uint16_t gain) { this->gain_voltage_ = gain; }
  void set_gain_ct(uint16_t gain) { this->gain_ct_ = gain; }
  void set_gain_pga(uint16_t gain) { gain_pga_ = gain; }
  void set_n_line_gain(uint16_t gain) { n_line_gain_ = gain; }

 protected:
  uint16_t read16_(uint8_t a_register);
  int read32_(uint8_t addr_h, uint8_t addr_l);
  void write16_(uint8_t a_register, uint16_t val);

  float get_line_voltage_();
  float get_line_current_();
  float get_active_power_();
  float get_reactive_power_();
  float get_power_factor_();
  float get_forward_active_energy_();
  float get_reverse_active_energy_();
  float get_frequency_();
  float get_chip_temperature_();

  sensor::Sensor *freq_sensor_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *reactive_power_sensor_{nullptr};
  sensor::Sensor *power_factor_sensor_{nullptr};
  sensor::Sensor *forward_active_energy_sensor_{nullptr};
  sensor::Sensor *reverse_active_energy_sensor_{nullptr};
  uint32_t cumulative_forward_active_energy_{0};
  uint32_t cumulative_reverse_active_energy_{0};
  uint16_t gain_metering_{7481};
  uint16_t gain_voltage_{26400};
  uint16_t gain_ct_{31251};
  uint16_t gain_pga_{0x4};
  uint16_t n_line_gain_{0x2};
  int line_freq_{60};
  float meter_constant_{3200.0f};
  uint32_t pl_const_{1429876};
};

}  // namespace atm90e26
}  // namespace esphome
