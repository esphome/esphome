#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace pmsx003 {

enum PMSX0003Command : uint8_t {
  PMS_CMD_MEASUREMENT_MODE =
      0xE1,  // Data Options: `PMS_CMD_MEASUREMENT_MODE_PASSIVE`, `PMS_CMD_MEASUREMENT_MODE_ACTIVE`
  PMS_CMD_MANUAL_MEASUREMENT = 0xE2,
  PMS_CMD_SLEEP_MODE = 0xE4,  // Data Options: `PMS_CMD_SLEEP_MODE_SLEEP`, `PMS_CMD_SLEEP_MODE_WAKEUP`
};

enum PMSX003Type {
  PMSX003_TYPE_X003 = 0,
  PMSX003_TYPE_5003T,
  PMSX003_TYPE_5003ST,
  PMSX003_TYPE_5003S,
};

enum PMSX003State {
  PMSX003_STATE_IDLE = 0,
  PMSX003_STATE_STABILISING,
  PMSX003_STATE_WAITING,
};

class PMSX003Component : public uart::UARTDevice, public Component {
 public:
  PMSX003Component() = default;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void dump_config() override;
  void loop() override;

  void set_update_interval(uint32_t update_interval) { this->update_interval_ = update_interval; }

  void set_type(PMSX003Type type) { this->type_ = type; }

  void set_pm_1_0_std_sensor(sensor::Sensor *pm_1_0_std_sensor) { this->pm_1_0_std_sensor_ = pm_1_0_std_sensor; }
  void set_pm_2_5_std_sensor(sensor::Sensor *pm_2_5_std_sensor) { this->pm_2_5_std_sensor_ = pm_2_5_std_sensor; }
  void set_pm_10_0_std_sensor(sensor::Sensor *pm_10_0_std_sensor) { this->pm_10_0_std_sensor_ = pm_10_0_std_sensor; }

  void set_pm_1_0_sensor(sensor::Sensor *pm_1_0_sensor) { this->pm_1_0_sensor_ = pm_1_0_sensor; }
  void set_pm_2_5_sensor(sensor::Sensor *pm_2_5_sensor) { this->pm_2_5_sensor_ = pm_2_5_sensor; }
  void set_pm_10_0_sensor(sensor::Sensor *pm_10_0_sensor) { this->pm_10_0_sensor_ = pm_10_0_sensor; }

  void set_pm_particles_03um_sensor(sensor::Sensor *pm_particles_03um_sensor) {
    this->pm_particles_03um_sensor_ = pm_particles_03um_sensor;
  }
  void set_pm_particles_05um_sensor(sensor::Sensor *pm_particles_05um_sensor) {
    this->pm_particles_05um_sensor_ = pm_particles_05um_sensor;
  }
  void set_pm_particles_10um_sensor(sensor::Sensor *pm_particles_10um_sensor) {
    this->pm_particles_10um_sensor_ = pm_particles_10um_sensor;
  }
  void set_pm_particles_25um_sensor(sensor::Sensor *pm_particles_25um_sensor) {
    this->pm_particles_25um_sensor_ = pm_particles_25um_sensor;
  }
  void set_pm_particles_50um_sensor(sensor::Sensor *pm_particles_50um_sensor) {
    this->pm_particles_50um_sensor_ = pm_particles_50um_sensor;
  }
  void set_pm_particles_100um_sensor(sensor::Sensor *pm_particles_100um_sensor) {
    this->pm_particles_100um_sensor_ = pm_particles_100um_sensor;
  }

  void set_formaldehyde_sensor(sensor::Sensor *formaldehyde_sensor) {
    this->formaldehyde_sensor_ = formaldehyde_sensor;
  }

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { this->humidity_sensor_ = humidity_sensor; }

 protected:
  optional<bool> check_byte_();
  void parse_data_();
  bool check_payload_length_(uint16_t payload_length);
  void send_command_(PMSX0003Command cmd, uint16_t data);
  uint16_t get_16_bit_uint_(uint8_t start_index);

  uint8_t data_[64];
  uint8_t data_index_{0};
  uint8_t initialised_{0};
  uint32_t fan_on_time_{0};
  uint32_t last_update_{0};
  uint32_t last_transmission_{0};
  uint32_t update_interval_{0};
  PMSX003State state_{PMSX003_STATE_IDLE};
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

  // Formaldehyde
  sensor::Sensor *formaldehyde_sensor_{nullptr};

  // Temperature and Humidity
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
};

}  // namespace pmsx003
}  // namespace esphome
