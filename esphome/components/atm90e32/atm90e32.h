#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace atm90e32 {

class ATM90E32Component : public PollingComponent,
  public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH,
  spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_200KHZ> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_voltage_sensor_a(sensor::Sensor *voltage_sensor) { voltage_sensor_a_ = voltage_sensor; }
  void set_voltage_sensor_b(sensor::Sensor *voltage_sensor) { voltage_sensor_b_ = voltage_sensor; }
  void set_voltage_sensor_c(sensor::Sensor *voltage_sensor) { voltage_sensor_c_ = voltage_sensor; }
  void set_current_sensor_a(sensor::Sensor *current_sensor) { current_sensor_a_ = current_sensor; }
  void set_current_sensor_b(sensor::Sensor *current_sensor) { current_sensor_b_ = current_sensor; }
  void set_current_sensor_c(sensor::Sensor *current_sensor) { current_sensor_c_ = current_sensor; }
  void set_freq_sensor(sensor::Sensor *freq_sensor) { freq_sensor_ = freq_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }
  void set_line_freq(int freq) { line_freq_ = freq; }
  void set_pga_gain(uint16_t gain) { pga_gain_ = gain; }
  void set_volt_a_gain(uint16_t gain) { volt_a_gain_ = gain; }
  void set_volt_b_gain(uint16_t gain) { volt_b_gain_ = gain; }
  void set_volt_c_gain(uint16_t gain) { volt_c_gain_ = gain; }
  void set_ct_a_gain(uint16_t gain) { ct_a_gain_ = gain; }
  void set_ct_b_gain(uint16_t gain) { ct_b_gain_ = gain; }
  void set_ct_c_gain(uint16_t gain) { ct_c_gain_ = gain; }

 protected:
  uint16_t read16_(uint16_t a_register);
  uint32_t read32_(uint16_t addr_h, uint16_t addr_l);
  void write16_(uint16_t a_register, uint16_t data);

  float GetLineVoltageA();
  float GetLineVoltageB();
  float GetLineVoltageC();
  float GetLineCurrentA();
  float GetLineCurrentB();
  float GetLineCurrentC();
  float GetTotalActivePower();
  float GetFrequency();

  sensor::Sensor *voltage_sensor_a_{nullptr};
  sensor::Sensor *voltage_sensor_b_{nullptr};
  sensor::Sensor *voltage_sensor_c_{nullptr};
  sensor::Sensor *current_sensor_a_{nullptr};
  sensor::Sensor *current_sensor_b_{nullptr};
  sensor::Sensor *current_sensor_c_{nullptr};
  sensor::Sensor *freq_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};

  uint16_t pga_gain_{0x15};
  uint16_t volt_a_gain_{41820};
  uint16_t volt_b_gain_{41820};
  uint16_t volt_c_gain_{41820};
  uint16_t ct_a_gain_{25498};
  uint16_t ct_b_gain_{25498};
  uint16_t ct_c_gain_{25498};
  int line_freq_{60};
};

}  // namespace atm90e32
}  // namespace esphome
