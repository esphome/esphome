#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::mmc5603 {

enum MMC5603Datarate {
  MMC5603_DATARATE_75_0_HZ,
  MMC5603_DATARATE_150_0_HZ,
  MMC5603_DATARATE_255_0_HZ,
};

class MMC5603Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(x)
  SUB_SENSOR(y)
  SUB_SENSOR(z)
  SUB_SENSOR(heading)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_datarate(MMC5603Datarate datarate) { datarate_ = datarate; }
  void set_auto_set_reset(bool auto_set_reset) { auto_set_reset_ = auto_set_reset; }

 protected:
  MMC5603Datarate datarate_{MMC5603_DATARATE_75_0_HZ};
  bool auto_set_reset_{true};
};

}  // namespace esphome::mmc5603
