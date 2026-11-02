#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/datatypes.h"

namespace esphome::cse7761 {

struct CSE7761DataStruct {
  uint32_t voltage_rms = 0;
  uint32_t current_rms[2] = {0};
  int32_t active_power[2] = {0};
  uint16_t coefficient[8] = {0};
  bool ready = false;
};

struct CSE7761EnergyChannel {
  uint32_t last_pulses{0};
  uint32_t pulses{0};
  float wh_per_pulse{0.0f};
};

enum class CSE7761CurrentInput : uint8_t {
  CSE7761_CURRENT_INPUT_SHUNT,
  CSE7761_CURRENT_INPUT_CURRENT_TRANSFORMER,
};

/// This class implements support for the CSE7761 UART power sensor.
class CSE7761Component final : public PollingComponent, public uart::UARTDevice {
 public:
  explicit CSE7761Component(CSE7761CurrentInput current_input) : current_input_(current_input) {}
  SUB_SENSOR(voltage)
  SUB_SENSOR(frequency)
  SUB_SENSOR(power_factor_1)
  SUB_SENSOR(active_power_1)
  SUB_SENSOR(current_1)
  void set_energy_1_sensor(sensor::Sensor *energy_sensor_1) { this->energy_sensors_[0] = energy_sensor_1; }
  SUB_SENSOR(active_power_2)
  SUB_SENSOR(current_2)
  void set_energy_2_sensor(sensor::Sensor *energy_sensor_2) { this->energy_sensors_[1] = energy_sensor_2; }
  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  // Sensors
  sensor::Sensor *energy_sensors_[2]{nullptr, nullptr};
  CSE7761DataStruct data_;
  CSE7761EnergyChannel energy_[2];
  const CSE7761CurrentInput current_input_;

  bool is_current_transformer_() const {
    return this->current_input_ == CSE7761CurrentInput::CSE7761_CURRENT_INPUT_CURRENT_TRANSFORMER;
  }
  void write_(uint8_t reg, uint16_t data);
  bool read_once_(uint8_t reg, uint8_t size, uint32_t *value);
  bool read_(uint8_t reg, uint8_t size, uint32_t *value);
  uint32_t read_(uint8_t reg, uint8_t size);
  uint32_t coefficient_by_unit_(uint32_t unit);
  bool chip_init_();
  void get_data_();
};

}  // namespace esphome::cse7761
