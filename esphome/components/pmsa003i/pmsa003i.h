#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::pmsa003i {

/**! Structure holding Plantower's standard packet **/
// From https://github.com/adafruit/Adafruit_PM25AQI
struct PM25AQIData {
  uint16_t framelen;         ///< How long this data chunk is
  uint16_t pm10_standard;    ///< Standard PM1.0
  uint16_t pm25_standard;    ///< Standard PM2.5
  uint16_t pm100_standard;   ///< Standard PM10.0
  uint16_t pm10_env;         ///< Environmental PM1.0
  uint16_t pm25_env;         ///< Environmental PM2.5
  uint16_t pm100_env;        ///< Environmental PM10.0
  uint16_t particles_03um;   ///< 0.3um Particle Count
  uint16_t particles_05um;   ///< 0.5um Particle Count
  uint16_t particles_10um;   ///< 1.0um Particle Count
  uint16_t particles_25um;   ///< 2.5um Particle Count
  uint16_t particles_50um;   ///< 5.0um Particle Count
  uint16_t particles_100um;  ///< 10.0um Particle Count
  uint16_t unused;           ///< Unused
  uint16_t checksum;         ///< Packet checksum
};

class PMSA003IComponent final : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_standard_units(bool standard_units) { this->standard_units_ = standard_units; }

 protected:
  void poll_until_ready_or_timeout_(uint32_t start_time);
  bool read_data_(PM25AQIData *data);

  bool standard_units_;

  SUB_SENSOR(pm_1_0)
  SUB_SENSOR(pm_2_5)
  SUB_SENSOR(pm_10_0)
  SUB_SENSOR(pmc_0_3)
  SUB_SENSOR(pmc_0_5)
  SUB_SENSOR(pmc_1_0)
  SUB_SENSOR(pmc_2_5)
  SUB_SENSOR(pmc_5_0)
  SUB_SENSOR(pmc_10_0)
};

}  // namespace esphome::pmsa003i
