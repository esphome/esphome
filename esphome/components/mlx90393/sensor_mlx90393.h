#pragma once

#include <MLX90393.h>
#include <MLX90393Hal.h>
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace mlx90393 {

enum MLX90393Setting {
  MLX90393_GAIN_SEL = 0,
  MLX90393_RESOLUTION,
  MLX90393_OVER_SAMPLING,
  MLX90393_DIGITAL_FILTERING,
  MLX90393_TEMPERATURE_OVER_SAMPLING,
  MLX90393_TEMPERATURE_COMPENSATION,
  MLX90393_HALLCONF,
  MLX90393_LAST,
};

class MLX90393Cls : public PollingComponent, public i2c::I2CDevice, public MLX90393Hal {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_drdy_gpio(GPIOPin *pin) { drdy_pin_ = pin; }

  void set_x_sensor(sensor::Sensor *x_sensor) { x_sensor_ = x_sensor; }
  void set_y_sensor(sensor::Sensor *y_sensor) { y_sensor_ = y_sensor; }
  void set_z_sensor(sensor::Sensor *z_sensor) { z_sensor_ = z_sensor; }
  void set_t_sensor(sensor::Sensor *t_sensor) { t_sensor_ = t_sensor; }

  void set_oversampling(uint8_t osr) { oversampling_ = osr; }
  void set_t_oversampling(uint8_t osr2) { temperature_oversampling_ = osr2; }
  void set_resolution(uint8_t xyz, uint8_t res) { resolutions_[xyz] = res; }
  void set_filter(uint8_t filter) { filter_ = filter; }
  void set_gain(uint8_t gain_sel) { gain_ = gain_sel; }
  void set_temperature_compensation(bool temperature_compensation) {
    temperature_compensation_ = temperature_compensation;
  }
  void set_hallconf(uint8_t hallconf) { hallconf_ = hallconf; }
  // overrides for MLX library

  // disable lint because it keeps suggesting const uint8_t *response.
  // this->read() writes data into response, so it can't be const
  bool transceive(const uint8_t *request, size_t request_size, uint8_t *response,
                  size_t response_size) override;  // NOLINT
  bool has_drdy_pin() override;
  bool read_drdy_pin() override;
  void sleep_millis(uint32_t millis) override;
  void sleep_micros(uint32_t micros) override;

 protected:
  MLX90393 mlx_;
  sensor::Sensor *x_sensor_{nullptr};
  sensor::Sensor *y_sensor_{nullptr};
  sensor::Sensor *z_sensor_{nullptr};
  sensor::Sensor *t_sensor_{nullptr};
  uint8_t gain_;
  uint8_t oversampling_;
  uint8_t temperature_oversampling_{0};
  uint8_t filter_;
  uint8_t resolutions_[3]{0};
  bool temperature_compensation_{false};
  uint8_t hallconf_{0xC};
  GPIOPin *drdy_pin_{nullptr};

  bool apply_all_settings_();
  uint8_t apply_setting_(MLX90393Setting which);

  bool verify_setting_(MLX90393Setting which);
  void verify_settings_timeout_(MLX90393Setting stage);
};

}  // namespace mlx90393
}  // namespace esphome
