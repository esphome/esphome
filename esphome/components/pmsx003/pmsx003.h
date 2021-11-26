#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace pmsx003 {

enum PMSX003Type {
  PMSX003_TYPE_X003 = 0,
  PMSX003_TYPE_5003T,
  PMSX003_TYPE_5003ST,
  PMSX003_TYPE_5003S,
};

class PMSX003Component : public uart::UARTDevice, public Component {
 public:
  PMSX003Component() = default;
  void loop() override;
  float get_setup_priority() const override;
  void dump_config() override;

  void set_type(PMSX003Type type) { type_ = type; }

  void set_pm_1_0_std_sensor(sensor::Sensor *pm_1_0_std_sensor);
  void set_pm_2_5_std_sensor(sensor::Sensor *pm_2_5_std_sensor);
  void set_pm_10_0_std_sensor(sensor::Sensor *pm_10_0_std_sensor);

  void set_pm_1_0_sensor(sensor::Sensor *pm_1_0_sensor);
  void set_pm_2_5_sensor(sensor::Sensor *pm_2_5_sensor);
  void set_pm_10_0_sensor(sensor::Sensor *pm_10_0_sensor);

  void set_pm_particles_03um_sensor(sensor::Sensor *pm_particles_03um_sensor);
  void set_pm_particles_05um_sensor(sensor::Sensor *pm_particles_05um_sensor);
  void set_pm_particles_10um_sensor(sensor::Sensor *pm_particles_10um_sensor);
  void set_pm_particles_25um_sensor(sensor::Sensor *pm_particles_25um_sensor);
  void set_pm_particles_50um_sensor(sensor::Sensor *pm_particles_50um_sensor);
  void set_pm_particles_100um_sensor(sensor::Sensor *pm_particles_100um_sensor);

  void set_temperature_sensor(sensor::Sensor *temperature_sensor);
  void set_humidity_sensor(sensor::Sensor *humidity_sensor);
  void set_formaldehyde_sensor(sensor::Sensor *formaldehyde_sensor);

 protected:
  optional<bool> check_byte_();
  void parse_data_();
  uint16_t get_16_bit_uint_(uint8_t start_index);

  uint8_t data_[64];
  uint8_t data_index_{0};
  uint32_t last_transmission_{0};
  PMSX003Type type_;

  // "Standard Particle"
  sensor::Sensor *pm_1_0_std_sensor_{nullptr};
  sensor::Sensor *pm_2_5_std_sensor_{nullptr};
  sensor::Sensor *pm_10_0_std_sensor_{nullptr};

  // "Under Atmospheric Pressure"
  sensor::Sensor *pm_1_0_sensor_{nullptr};
  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};

  // Particle counts by size
  sensor::Sensor *pm_particles_03um_sensor_{nullptr};
  sensor::Sensor *pm_particles_05um_sensor_{nullptr};
  sensor::Sensor *pm_particles_10um_sensor_{nullptr};
  sensor::Sensor *pm_particles_25um_sensor_{nullptr};
  sensor::Sensor *pm_particles_50um_sensor_{nullptr};
  sensor::Sensor *pm_particles_100um_sensor_{nullptr};

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *formaldehyde_sensor_{nullptr};
};

}  // namespace pmsx003
}  // namespace esphome
