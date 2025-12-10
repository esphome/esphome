#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/gpio.h"
#include "xensiv_pas_co2_regs.h"

namespace esphome {
namespace xensiv_pas_co2_base {

class XensivPasCO2 : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void read_co2_ppm();

  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }
  void set_interrupt_pin(InternalGPIOPin *pin) { interrupt_pin_ = pin; }
  void set_sensor_rate_value(int16_t rate) { sensor_rate_ = rate; }
  void set_operation_mode(bool mode) { continuous_operation_mode_ = mode; }
  void set_pressure_compensation(uint16_t pressure_ref);
  void set_pressure_compensation_source(sensor::Sensor *sensor) { pressure_compensation_source_ = sensor; }
  bool measure_now();
  void reset_ABOC();

 protected:
  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *pressure_compensation_source_{nullptr};
  float co2_ppm_{0.0f};
  uint16_t version_{2};
  uint16_t pressure_ref_{0};              // Pressure reference in Pa (0 = use default from sensor)
  int16_t sensor_rate_{10};               // Default rate in seconds
  bool continuous_operation_mode_{true};  // Default: continuous mode
  bool initialized_{false};               // Sensor initialization complete flag

  static void gpio_intr(XensivPasCO2 *arg);
  static void setup_sensor(XensivPasCO2 *arg);
  bool test_scratch_register_();
  bool update_operation_mode_();
  bool update_sensor_rate_();
  bool check_sensor_ready_();
  bool setup_interrupt_();

  virtual bool read_byte(uint8_t reg, uint8_t *data) = 0;
  virtual bool read_bytes(uint8_t reg, uint8_t *data, size_t len) = 0;
  virtual bool write_byte(uint8_t reg, uint8_t value) = 0;

  InternalGPIOPin *interrupt_pin_{nullptr};
  volatile bool data_ready_{false};
};

}  // namespace xensiv_pas_co2_base
}  // namespace esphome
