#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"


/*=========================================================================
    I2C ADDRESS/BITS
    -----------------------------------------------------------------------*/
#define MMC56X3_DEFAULT_ADDRESS 0x30 //!< Default address
#define MMC56X3_CHIP_ID 0x10         //!< Chip ID from WHO_AM_I register

/*=========================================================================*/

namespace esphome {
namespace mmc5603 {


enum MMC5603Datarate {
  MMC5603_DATARATE_75_0_HZ,
  MMC5603_DATARATE_150_0_HZ,
  MMC5603_DATARATE_255_0_HZ,
};


class MMC5603Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_datarate(MMC5603Datarate datarate) { datarate_ = datarate; }
  void set_x_sensor(sensor::Sensor *x_sensor) { x_sensor_ = x_sensor; }
  void set_y_sensor(sensor::Sensor *y_sensor) { y_sensor_ = y_sensor; }
  void set_z_sensor(sensor::Sensor *z_sensor) { z_sensor_ = z_sensor; }
  void set_heading_sensor(sensor::Sensor *heading_sensor) { heading_sensor_ = heading_sensor; }

 protected:
  MMC5603Datarate datarate_{MMC5603_DATARATE_150_0_HZ};
  sensor::Sensor *x_sensor_{nullptr};
  sensor::Sensor *y_sensor_{nullptr};
  sensor::Sensor *z_sensor_{nullptr};
  sensor::Sensor *heading_sensor_{nullptr};
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    ID_REGISTERS,
  } error_code_;
};

}  // namespace hmc5883l
}  // namespace esphome
