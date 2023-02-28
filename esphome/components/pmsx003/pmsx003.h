#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace pmsx003 {

// known command bytes
#define PMS_CMD_PASSIVE_ACTIVE 0xE1  // data=0: perform measurement manually, data=1: perform measurements continuously
#define PMS_CMD_REQUEST_READ 0xE2    // trigger a manual measurement
#define PMS_CMD_SLEEP_WAKEUP 0xE4    // data=0: sleep, data=1: wakeup

enum PMSX003Type {
  PMSX003_TYPE_X003 = 0,
  PMSX003_TYPE_5003T,
  PMSX003_TYPE_5003ST,
  PMSX003_TYPE_5003S,
};

class PMSX003Component : public uart::UARTDevice, public PollingComponent {
 public:
  PMSX003Component() = default;
  void setup() override;
  void update() override;
  float get_setup_priority() const override;
  void dump_config() override;

  void set_type(PMSX003Type type) { type_ = type; }
  void set_warmup_interval(uint32_t warmup_interval) { warmup_interval_ = warmup_interval; }

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
  void take_measurement_(uint32_t timeout = 1000);
  optional<bool> check_byte_();
  void parse_data_();
  void send_command_(uint8_t cmd, uint16_t data);
  uint16_t get_16_bit_uint_(uint8_t start_index);

  uint8_t data_[64];
  uint8_t data_index_{0};
  PMSX003Type type_;
  uint32_t warmup_interval_{30000};
  optional<uint32_t> started_at_{};
  bool warmed_up_{false};
  bool is_laser_save_mode_{false};

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
