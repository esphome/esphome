#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::pmsx003 {

enum class Type : uint8_t {
  PMS1003 = 0,
  PMS3003,
  PMSX003,  // PMS5003, PMS6003, PMS7003, PMSA003 (NOT PMSA003I - see `pmsa003i` component)
  PMS5003S,
  PMS5003T,
  PMS5003ST,
  PMS9003M,
};

enum class Command : uint8_t {
  MEASUREMENT_MODE = 0xE1,  // Data Options: `CMD_MEASUREMENT_MODE_PASSIVE`, `CMD_MEASUREMENT_MODE_ACTIVE`
  MANUAL_MEASUREMENT = 0xE2,
  SLEEP_MODE = 0xE4,  // Data Options: `CMD_SLEEP_MODE_SLEEP`, `CMD_SLEEP_MODE_WAKEUP`
};

enum class State : uint8_t {
  IDLE = 0,
  STABILISING,
  WAITING,
};

class PMSX003Component : public uart::UARTDevice, public Component {
 public:
  PMSX003Component() = default;
  void setup() override;
  void dump_config() override;
  void loop() override;

  void set_update_interval(uint32_t update_interval) { this->update_interval_ = update_interval; }

  void set_type(Type type) { this->type_ = type; }

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
  void send_command_(Command cmd, uint16_t data);
  uint16_t get_16_bit_uint_(uint8_t start_index) const {
    return encode_uint16(this->data_[start_index], this->data_[start_index + 1]);
  }

  Type type_;
  State state_{State::IDLE};
  bool initialised_{false};
  uint8_t data_[64];
  uint8_t data_index_{0};
  uint32_t fan_on_time_{0};
  uint32_t last_update_{0};
  uint32_t last_transmission_{0};
  uint32_t update_interval_{0};

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

}  // namespace esphome::pmsx003
