#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace gcja5 {

/**! Structure holding Plantower's standard packet **/
// From https://github.com/adafruit/Adafruit_PM25AQI
// Modified for gcja5 to have three 32-bit standards
struct PM25AQIData {
  uint32_t pm10_standard,   ///< Standard PM1.0
      pm25_standard,        ///< Standard PM2.5
      pm100_standard;       ///< Standard PM10.0
  uint16_t particles_03um,  ///< 0.3um Particle Count
      particles_05um,       ///< 0.5um Particle Count
      particles_10um,       ///< 1.0um Particle Count
      particles_25um,       ///< 2.5um Particle Count
      particles_50um,       ///< 5.0um Particle Count
      particles_100um;      ///< 10.0um Particle Count
  uint8_t status;           ///< Status information
};

enum SNGCJA5_REGISTERS {
  SNGCJA5_PM1_0 = 0x00,
  SNGCJA5_PM2_5 = 0x04,
  SNGCJA5_PM10 = 0x08,
  SNGCJA5_PCOUNT_0_5 = 0x0C,
  SNGCJA5_PCOUNT_1_0 = 0x0E,
  SNGCJA5_PCOUNT_2_5 = 0x10,
  SNGCJA5_PCOUNT_5_0 = 0x14,
  SNGCJA5_PCOUNT_7_5 = 0x16,
  SNGCJA5_PCOUNT_10 = 0x18,
  SNGCJA5_STATE = 0x26,
};

class GCJA5Component : public PollingComponent, public i2c::I2CDevice {
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
  uint8_t get_status(PM25AQIData *data);
  uint8_t get_status_pd(PM25AQIData *data);
  uint8_t get_status_ld(PM25AQIData *data);
  uint8_t get_status_fan(PM25AQIData *data);
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

}  // namespace gcja5
}  // namespace esphome
