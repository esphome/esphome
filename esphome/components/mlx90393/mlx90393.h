#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mlx90393 {

enum MLX90393Gain {
  MLX9039_GAIN_5 = 0,
  MLX9039_GAIN_4 = 1,
  MLX9039_GAIN_3 = 2,
  MLX9039_GAIN_2P5 = 3,
  MLX9039_GAIN_2 = 4,
  MLX9039_GAIN_1P67 = 5,
  MLX9039_GAIN_1P33 = 6,
  MLX9039_GAIN_1 = 7,
};

enum MLX90393Oversampling {
  MLX9039_OVERSAMPLING_NONE = 0,
  MLX9039_OVERSAMPLING_2X = 1,
  MLX9039_OVERSAMPLING_4X = 2,
  MLX9039_OVERSAMPLING_8X = 3,
};

enum MLX90393Range {
  MLX9039_RANGE_16BIT = 0,
  MLX9039_RANGE_17BIT = 1,
  MLX9039_RANGE_18BIT = 2,
  MLX9039_RANGE_19BIT = 3,
};

class MLX90393Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_gain(MLX90393Gain gain) { gain_ = gain; }
  void set_range(MLX90393Range range) { range_ = range; }
  void set_oversampling(MLX90393Oversampling oversampling) { oversampling_ = oversampling; }
  void set_x_sensor(sensor::Sensor *x_sensor) { x_sensor_ = x_sensor; }
  void set_y_sensor(sensor::Sensor *y_sensor) { y_sensor_ = y_sensor; }
  void set_z_sensor(sensor::Sensor *z_sensor) { z_sensor_ = z_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }

 protected:
  bool write_register_(uint8_t a_register, uint16_t value);
  optional<uint16_t> read_register_(uint8_t a_register);

  MLX90393Gain gain_{MLX9039_GAIN_1};
  MLX90393Oversampling oversampling_{MLX9039_OVERSAMPLING_8X};
  MLX90393Range range_{MLX9039_RANGE_18BIT};
  sensor::Sensor *x_sensor_;
  sensor::Sensor *y_sensor_;
  sensor::Sensor *z_sensor_;
  sensor::Sensor *temperature_sensor_;
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    RESET_FAILED,
    CONFIGURE_FAILED,
  } error_code_{NONE};
  uint16_t tref_;
};

}  // namespace mlx90393
}  // namespace esphome
