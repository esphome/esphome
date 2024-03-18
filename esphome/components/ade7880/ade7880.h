#pragma once

// This component was developed using knowledge gathered by a number
// of people who reverse-engineered the Shelly 3EM:
//
// @AndreKR on GitHub
// Axel (@Axel830 on GitHub)
// Marko (@goodkiller on GitHub)
// Michaël Piron (@michaelpiron on GitHub)
// Theo Arends (@arendst on GitHub)

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

#include "ade7880_registers.h"

namespace esphome {
namespace ade7880 {

struct NeutralChannel {
  void set_current(sensor::Sensor *sens) { this->current = sens; }

  void set_current_gain_calibration(int32_t val) { this->current_gain_calibration = val; }

  sensor::Sensor *current{nullptr};
  int32_t current_gain_calibration{0};
};

struct PowerChannel {
  void set_current(sensor::Sensor *sens) { this->current = sens; }
  void set_voltage(sensor::Sensor *sens) { this->voltage = sens; }
  void set_active_power(sensor::Sensor *sens) { this->active_power = sens; }
  void set_apparent_power(sensor::Sensor *sens) { this->apparent_power = sens; }
  void set_power_factor(sensor::Sensor *sens) { this->power_factor = sens; }
  void set_forward_active_energy(sensor::Sensor *sens) { this->forward_active_energy = sens; }
  void set_reverse_active_energy(sensor::Sensor *sens) { this->reverse_active_energy = sens; }

  void set_current_gain_calibration(int32_t val) { this->current_gain_calibration = val; }
  void set_voltage_gain_calibration(int32_t val) { this->voltage_gain_calibration = val; }
  void set_power_gain_calibration(int32_t val) { this->power_gain_calibration = val; }
  void set_phase_angle_calibration(int32_t val) { this->phase_angle_calibration = val; }

  sensor::Sensor *current{nullptr};
  sensor::Sensor *voltage{nullptr};
  sensor::Sensor *active_power{nullptr};
  sensor::Sensor *apparent_power{nullptr};
  sensor::Sensor *power_factor{nullptr};
  sensor::Sensor *forward_active_energy{nullptr};
  sensor::Sensor *reverse_active_energy{nullptr};
  int32_t current_gain_calibration{0};
  int32_t voltage_gain_calibration{0};
  int32_t power_gain_calibration{0};
  uint16_t phase_angle_calibration{0};
  float forward_active_energy_total{0};
  float reverse_active_energy_total{0};
};

// Store data in a class that doesn't use multiple-inheritance (no vtables in flash!)
struct ADE7880Store {
  volatile bool reset_done{false};
  bool reset_pending{false};
  ISRInternalGPIOPin irq1_pin;

  static void gpio_intr(ADE7880Store *arg);
};

class ADE7880 : public i2c::I2CDevice, public PollingComponent {
 public:
  void set_irq0_pin(InternalGPIOPin *pin) { this->irq0_pin_ = pin; }
  void set_irq1_pin(InternalGPIOPin *pin) { this->irq1_pin_ = pin; }
  void set_reset_pin(InternalGPIOPin *pin) { this->reset_pin_ = pin; }
  void set_frequency(float frequency) { this->frequency_ = frequency; }
  void set_channel_n(NeutralChannel *channel) { this->channel_n_ = channel; }
  void set_channel_a(PowerChannel *channel) { this->channel_a_ = channel; }
  void set_channel_b(PowerChannel *channel) { this->channel_b_ = channel; }
  void set_channel_c(PowerChannel *channel) { this->channel_c_ = channel; }

  void setup() override;

  void loop() override;

  void update() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  ADE7880Store store_{};
  InternalGPIOPin *irq0_pin_{nullptr};
  InternalGPIOPin *irq1_pin_{nullptr};
  InternalGPIOPin *reset_pin_{nullptr};
  float frequency_;
  NeutralChannel *channel_n_{nullptr};
  PowerChannel *channel_a_{nullptr};
  PowerChannel *channel_b_{nullptr};
  PowerChannel *channel_c_{nullptr};

  void calibrate_s10zp_reading_(uint16_t a_register, int16_t calibration);
  void calibrate_s24zpse_reading_(uint16_t a_register, int32_t calibration);

  void init_device_();

  // each of these functions allow the caller to pass in a lambda (or any other callable)
  // which modifies the value read from the register before it is passed to the sensor
  // the callable will be passed a 'float' value and is expected to return a 'float'
  template<typename F> void update_sensor_from_s24zp_register16_(sensor::Sensor *sensor, uint16_t a_register, F &&f);
  template<typename F> void update_sensor_from_s16_register16_(sensor::Sensor *sensor, uint16_t a_register, F &&f);
  template<typename F> void update_sensor_from_s32_register16_(sensor::Sensor *sensor, uint16_t a_register, F &&f);

  void reset_device_();

  uint8_t read_u8_register16_(uint16_t a_register);
  int16_t read_s16_register16_(uint16_t a_register);
  uint16_t read_u16_register16_(uint16_t a_register);
  int32_t read_s24zp_register16_(uint16_t a_register);
  int32_t read_s32_register16_(uint16_t a_register);
  uint32_t read_u32_register16_(uint16_t a_register);

  void write_u8_register16_(uint16_t a_register, uint8_t value);
  void write_s10zp_register16_(uint16_t a_register, int16_t value);
  void write_u16_register16_(uint16_t a_register, uint16_t value);
  void write_s24zpse_register16_(uint16_t a_register, int32_t value);
  void write_s32_register16_(uint16_t a_register, int32_t value);
  void write_u32_register16_(uint16_t a_register, uint32_t value);
};

}  // namespace ade7880
}  // namespace esphome
