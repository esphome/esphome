#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/pulse_counter/pulse_counter_sensor.h"

#include <cinttypes>

namespace esphome::hlw8012 {

enum HLW8012InitialMode { HLW8012_INITIAL_MODE_CURRENT = 0, HLW8012_INITIAL_MODE_VOLTAGE };

enum HLW8012SensorModels {
  HLW8012_SENSOR_MODEL_HLW8012 = 0,
  HLW8012_SENSOR_MODEL_CSE7759,
  HLW8012_SENSOR_MODEL_BL0937
};

class HLW8012Component final : public PollingComponent {
  SUB_SENSOR(voltage)
  SUB_SENSOR(current)
  SUB_SENSOR(power)
  SUB_SENSOR(energy)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_initial_mode(HLW8012InitialMode initial_mode) {
    current_mode_ = initial_mode == HLW8012_INITIAL_MODE_CURRENT;
  }
  void set_sensor_model(HLW8012SensorModels sensor_model) { sensor_model_ = sensor_model; }
  void set_change_mode_every(uint32_t change_mode_every) { change_mode_every_ = change_mode_every; }
  void set_current_resistor(float current_resistor) { current_resistor_ = current_resistor; }
  void set_voltage_divider(float voltage_divider) { voltage_divider_ = voltage_divider; }
  void set_sel_pin(GPIOPin *sel_pin) { sel_pin_ = sel_pin; }
  void set_cf_pin(InternalGPIOPin *cf_pin) { cf_pin_ = cf_pin; }
  void set_cf1_pin(InternalGPIOPin *cf1_pin) { cf1_pin_ = cf1_pin; }

 protected:
#ifdef HAS_PCNT
  using Storage = pulse_counter::HwPulseCounterStorage;
#else
  using Storage = pulse_counter::BasicPulseCounterStorage;
#endif

  uint32_t nth_value_{0};
  bool current_mode_{false};
  uint32_t change_mode_at_{0};
  uint32_t change_mode_every_{8};
  float current_resistor_{0.001};
  float voltage_divider_{2351};
  HLW8012SensorModels sensor_model_{HLW8012_SENSOR_MODEL_HLW8012};
  uint64_t cf_total_pulses_{0};
  GPIOPin *sel_pin_;
  InternalGPIOPin *cf_pin_;
  Storage cf_store_;
  InternalGPIOPin *cf1_pin_;
  Storage cf1_store_;

  float voltage_multiplier_{0.0f};
  float current_multiplier_{0.0f};
  float power_multiplier_{0.0f};
};

}  // namespace esphome::hlw8012
