#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace pmsa003i {

/**! Structure holding Plantower's standard packet **/
// From https://github.com/adafruit/Adafruit_PM25AQI
struct PM25AQIData {
  uint16_t framelen;        ///< How long this data chunk is
  uint16_t pm10_standard,   ///< Standard PM1.0
      pm25_standard,        ///< Standard PM2.5
      pm100_standard;       ///< Standard PM10.0
  uint16_t pm10_env,        ///< Environmental PM1.0
      pm25_env,             ///< Environmental PM2.5
      pm100_env;            ///< Environmental PM10.0
  uint16_t particles_03um,  ///> 0.3um Particle Count
      particles_05um,       ///> 0.5um Particle Count
      particles_10um,       ///> 1.0um Particle Count
      particles_25um,       ///> 2.5um Particle Count
      particles_50um,       ///> 5.0um Particle Count
      particles_100um;      ///> 10.0um Particle Count
  uint16_t unused;          ///< Unused
  uint16_t checksum;        ///< Packet checksum
};

class PMSA003IComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_standard_units(bool standard_units) { standard_units_ = standard_units; }

  void set_pm_1_0_sensor(sensor::Sensor *pm_1_0) { pm_1_0_sensor_ = pm_1_0; }
  void set_pm_2_5_sensor(sensor::Sensor *pm_2_5) { pm_2_5_sensor_ = pm_2_5; }
  void set_pm_10_0_sensor(sensor::Sensor *pm_10_0) { pm_10_0_sensor_ = pm_10_0; }

  void set_pmc_0_3_sensor(sensor::Sensor *pmc_0_3) { pmc_0_3_sensor_ = pmc_0_3; }
  void set_pmc_0_5_sensor(sensor::Sensor *pmc_0_5) { pmc_0_5_sensor_ = pmc_0_5; }
  void set_pmc_1_0_sensor(sensor::Sensor *pmc_1_0) { pmc_1_0_sensor_ = pmc_1_0; }
  void set_pmc_2_5_sensor(sensor::Sensor *pmc_2_5) { pmc_2_5_sensor_ = pmc_2_5; }
  void set_pmc_5_0_sensor(sensor::Sensor *pmc_5_0) { pmc_5_0_sensor_ = pmc_5_0; }
  void set_pmc_10_0_sensor(sensor::Sensor *pmc_10_0) { pmc_10_0_sensor_ = pmc_10_0; }

 protected:
  bool read_data_(PM25AQIData *data);

  bool standard_units_;

  sensor::Sensor *pm_1_0_sensor_{nullptr};
  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};

  sensor::Sensor *pmc_0_3_sensor_{nullptr};
  sensor::Sensor *pmc_0_5_sensor_{nullptr};
  sensor::Sensor *pmc_1_0_sensor_{nullptr};
  sensor::Sensor *pmc_2_5_sensor_{nullptr};
  sensor::Sensor *pmc_5_0_sensor_{nullptr};
  sensor::Sensor *pmc_10_0_sensor_{nullptr};
};

}  // namespace pmsa003i
}  // namespace esphome
