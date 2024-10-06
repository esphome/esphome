#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace pmsx003 {

// known command bytes
static const uint8_t PMS_CMD_AUTO_MANUAL =
    0xE1;  // data=0: perform measurement manually, data=1: perform measurement automatically
static const uint8_t PMS_CMD_TRIG_MANUAL = 0xE2;  // trigger a manual measurement
static const uint8_t PMS_CMD_ON_STANDBY = 0xE4;   // data=0: go to standby mode, data=1: go to normal mode

static const uint16_t PMS_STABILISING_MS = 30000;  // time taken for the sensor to become stable after power on

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
  void loop() override;
  float get_setup_priority() const override;
  void dump_config() override;

  void set_type(PMSX003Type type) { type_ = type; }

  void set_update_interval(uint32_t val) { update_interval_ = val; };

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
  void send_command_(uint8_t cmd, uint16_t data);
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

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *formaldehyde_sensor_{nullptr};
};

}  // namespace pmsx003
}  // namespace esphome
