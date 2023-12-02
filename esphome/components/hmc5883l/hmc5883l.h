#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace hmc5883l {

enum HMC5883LOversampling {
  HMC5883L_OVERSAMPLING_1 = 0b000,
  HMC5883L_OVERSAMPLING_2 = 0b001,
  HMC5883L_OVERSAMPLING_4 = 0b010,
  HMC5883L_OVERSAMPLING_8 = 0b011,
};

enum HMC5883LDatarate {
  HMC5883L_DATARATE_0_75_HZ = 0b000,
  HMC5883L_DATARATE_1_5_HZ = 0b001,
  HMC5883L_DATARATE_3_0_HZ = 0b010,
  HMC5883L_DATARATE_7_5_HZ = 0b011,
  HMC5883L_DATARATE_15_0_HZ = 0b100,
  HMC5883L_DATARATE_30_0_HZ = 0b101,
  HMC5883L_DATARATE_75_0_HZ = 0b110,
};

enum HMC5883LRange {
  HMC5883L_RANGE_88_UT = 0b000,
  HMC5883L_RANGE_130_UT = 0b001,
  HMC5883L_RANGE_190_UT = 0b010,
  HMC5883L_RANGE_250_UT = 0b011,
  HMC5883L_RANGE_400_UT = 0b100,
  HMC5883L_RANGE_470_UT = 0b101,
  HMC5883L_RANGE_560_UT = 0b110,
  HMC5883L_RANGE_810_UT = 0b111,
};

class HMC5883LComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_oversampling(HMC5883LOversampling oversampling) { oversampling_ = oversampling; }
  void set_datarate(HMC5883LDatarate datarate) { datarate_ = datarate; }
  void set_range(HMC5883LRange range) { range_ = range; }
  void set_x_sensor(sensor::Sensor *x_sensor) { x_sensor_ = x_sensor; }
  void set_y_sensor(sensor::Sensor *y_sensor) { y_sensor_ = y_sensor; }
  void set_z_sensor(sensor::Sensor *z_sensor) { z_sensor_ = z_sensor; }
  void set_heading_sensor(sensor::Sensor *heading_sensor) { heading_sensor_ = heading_sensor; }

 protected:
  HMC5883LOversampling oversampling_{HMC5883L_OVERSAMPLING_1};
  HMC5883LDatarate datarate_{HMC5883L_DATARATE_15_0_HZ};
  HMC5883LRange range_{HMC5883L_RANGE_130_UT};
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
