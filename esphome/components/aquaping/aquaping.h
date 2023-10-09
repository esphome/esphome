#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome {
namespace aquaping {

class AQUAPINGComponent : public PollingComponent, public i2c::I2CDevice {

  #ifdef USE_SENSOR
     SUB_SENSOR(quiet_count)
     SUB_SENSOR(leak_count)
     SUB_SENSOR(noise_count)
     SUB_SENSOR(version)
  #endif

  #ifdef USE_BINARY_SENSOR
   SUB_BINARY_SENSOR(alarm)
   SUB_BINARY_SENSOR(noise_alert)
   SUB_BINARY_SENSOR(led)
   SUB_BINARY_SENSOR(sleep)
  #endif



  // TODO
  // Background size
  // Event size
  // Trigger size
  // Period
  // Version

 public:
 float get_setup_priority() const override { return setup_priority::DATA; }
 void setup() override;
 void dump_config() override;
 void update() override;
};

}  // namespace aquaping
}  // namespace esphome
