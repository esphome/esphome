#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ips7100 {

class IPS7100Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  // PM mass concentration setters (µg/m³)
  void set_pm_0_1_sensor(sensor::Sensor *sensor) { this->pm_0_1_sensor_ = sensor; }
  void set_pm_0_3_sensor(sensor::Sensor *sensor) { this->pm_0_3_sensor_ = sensor; }
  void set_pm_0_5_sensor(sensor::Sensor *sensor) { this->pm_0_5_sensor_ = sensor; }
  void set_pm_1_0_sensor(sensor::Sensor *sensor) { this->pm_1_0_sensor_ = sensor; }
  void set_pm_2_5_sensor(sensor::Sensor *sensor) { this->pm_2_5_sensor_ = sensor; }
  void set_pm_5_0_sensor(sensor::Sensor *sensor) { this->pm_5_0_sensor_ = sensor; }
  void set_pm_10_0_sensor(sensor::Sensor *sensor) { this->pm_10_0_sensor_ = sensor; }

  // Particle count setters (#/cm³)
  void set_pmc_0_1_sensor(sensor::Sensor *sensor) { this->pmc_0_1_sensor_ = sensor; }
  void set_pmc_0_3_sensor(sensor::Sensor *sensor) { this->pmc_0_3_sensor_ = sensor; }
  void set_pmc_0_5_sensor(sensor::Sensor *sensor) { this->pmc_0_5_sensor_ = sensor; }
  void set_pmc_1_0_sensor(sensor::Sensor *sensor) { this->pmc_1_0_sensor_ = sensor; }
  void set_pmc_2_5_sensor(sensor::Sensor *sensor) { this->pmc_2_5_sensor_ = sensor; }
  void set_pmc_5_0_sensor(sensor::Sensor *sensor) { this->pmc_5_0_sensor_ = sensor; }
  void set_pmc_10_0_sensor(sensor::Sensor *sensor) { this->pmc_10_0_sensor_ = sensor; }

 protected:
  bool read_pm_data_();
  bool read_pc_data_();

  // PM mass concentration sensors
  sensor::Sensor *pm_0_1_sensor_{nullptr};
  sensor::Sensor *pm_0_3_sensor_{nullptr};
  sensor::Sensor *pm_0_5_sensor_{nullptr};
  sensor::Sensor *pm_1_0_sensor_{nullptr};
  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_5_0_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};

  // Particle count sensors
  sensor::Sensor *pmc_0_1_sensor_{nullptr};
  sensor::Sensor *pmc_0_3_sensor_{nullptr};
  sensor::Sensor *pmc_0_5_sensor_{nullptr};
  sensor::Sensor *pmc_1_0_sensor_{nullptr};
  sensor::Sensor *pmc_2_5_sensor_{nullptr};
  sensor::Sensor *pmc_5_0_sensor_{nullptr};
  sensor::Sensor *pmc_10_0_sensor_{nullptr};

  // Data storage
  float pm_values_[7]{0};
  uint32_t pc_values_[7]{0};
};

}  // namespace ips7100
}  // namespace esphome
