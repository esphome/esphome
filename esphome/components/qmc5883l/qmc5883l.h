#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace qmc5883l {

enum QMC5883LDatarate {
  QMC5883L_DATARATE_10_HZ = 0b00,
  QMC5883L_DATARATE_50_HZ = 0b01,
  QMC5883L_DATARATE_100_HZ = 0b10,
  QMC5883L_DATARATE_200_HZ = 0b11,
};

enum QMC5883LRange {
  QMC5883L_RANGE_200_UT = 0b00,
  QMC5883L_RANGE_800_UT = 0b01,
};

enum QMC5883LOversampling {
  QMC5883L_SAMPLING_512 = 0b00,
  QMC5883L_SAMPLING_256 = 0b01,
  QMC5883L_SAMPLING_128 = 0b10,
  QMC5883L_SAMPLING_64 = 0b11,
};

class QMC5883LComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_datarate(QMC5883LDatarate datarate) { datarate_ = datarate; }
  void set_range(QMC5883LRange range) { range_ = range; }
  void set_oversampling(QMC5883LOversampling oversampling) { oversampling_ = oversampling; }
  void set_x_sensor(sensor::Sensor *x_sensor) { x_sensor_ = x_sensor; }
  void set_y_sensor(sensor::Sensor *y_sensor) { y_sensor_ = y_sensor; }
  void set_z_sensor(sensor::Sensor *z_sensor) { z_sensor_ = z_sensor; }
  void set_heading_sensor(sensor::Sensor *heading_sensor) { heading_sensor_ = heading_sensor; }

 protected:
  QMC5883LDatarate datarate_{QMC5883L_DATARATE_10_HZ};
  QMC5883LRange range_{QMC5883L_RANGE_200_UT};
  QMC5883LOversampling oversampling_{QMC5883L_SAMPLING_512};
  sensor::Sensor *x_sensor_;
  sensor::Sensor *y_sensor_;
  sensor::Sensor *z_sensor_;
  sensor::Sensor *heading_sensor_;
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
  } error_code_;
  bool read_byte_16_(uint8_t a_register, uint16_t *data);
};

}  // namespace qmc5883l
}  // namespace esphome
