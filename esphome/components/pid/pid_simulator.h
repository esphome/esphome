#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/output/float_output.h"

#include <vector>

namespace esphome {
namespace pid {

class PIDSimulator : public PollingComponent, public output::FloatOutput {
 public:
  PIDSimulator() : PollingComponent(1000) {}

  float surface = 1;                     /// surface area in m²
  float mass = 3;                        /// mass of simulated object in kg
  float temperature = 21;                /// current temperature of object in °C
  float efficiency = 0.98;               /// heating efficiency, 1 is 100% efficient
  float thermal_conductivity = 15;       /// thermal conductivity of surface are in W/(m*K), here: steel
  float specific_heat_capacity = 4.182;  /// specific heat capacity of mass in kJ/(kg*K), here: water
  float heat_power = 500;                /// Heating power in W
  float ambient_temperature = 20;        /// Ambient temperature in °C
  float update_interval = 1;             /// The simulated updated interval in seconds
  std::vector<float> delayed_temps;      /// storage of past temperatures for delaying temperature reading
  size_t delay_cycles = 15;              /// how many update cycles to delay the output
  float output_value = 0.0;              /// Current output value of heating element
  sensor::Sensor *sensor = new sensor::Sensor();

  float delta_t(float power) {
    // P = Q / t
    // Q = c * m * 𝚫t
    // 𝚫t = (P*t) / (c*m)
    float c = this->specific_heat_capacity;
    float t = this->update_interval;
    float p = power / 1000;  //  in kW
    float m = this->mass;
    return (p * t) / (c * m);
  }

  float update_temp() {
    float value = clamp(output_value, 0.0f, 1.0f);

    // Heat
    float power = value * heat_power * efficiency;
    temperature += this->delta_t(power);

    // Cool
    // Q = k_w * A * (T_mass - T_ambient)
    // P = Q / t
    float dt = temperature - ambient_temperature;
    float cool_power = (thermal_conductivity * surface * dt) / update_interval;
    temperature -= this->delta_t(cool_power);

    // Delay temperature readings
    delayed_temps.push_back(temperature);
    if (delayed_temps.size() > delay_cycles)
      delayed_temps.erase(delayed_temps.begin());
    float prev_temp = this->delayed_temps[0];
    float alpha = 0.1f;
    float ret = (1 - alpha) * prev_temp + alpha * prev_temp;
    return ret;
  }

  void setup() override { sensor->publish_state(this->temperature); }
  void update() override {
    float new_temp = this->update_temp();
    sensor->publish_state(new_temp);
  }

 protected:
  void write_state(float state) override { this->output_value = state; }
};

}  // namespace pid
}  // namespace esphome
