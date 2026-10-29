#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/optional.h"

namespace esphome::ltr390 {

enum LTR390CTRL {
  LTR390_CTRL_EN = 1,
  LTR390_CTRL_MODE = 3,
  LTR390_CTRL_RST = 4,
};

// enums from https://github.com/adafruit/Adafruit_LTR390/
// Sensing modes
enum LTR390MODE : uint8_t {
  LTR390_MODE_ALS,
  LTR390_MODE_UVS,
};

// Sensor gain levels
enum LTR390GAIN : uint8_t {
  LTR390_GAIN_1 = 0,
  LTR390_GAIN_3,  // Default
  LTR390_GAIN_6,
  LTR390_GAIN_9,
  LTR390_GAIN_18,
};

// Sensor resolution
enum LTR390RESOLUTION : uint8_t {
  LTR390_RESOLUTION_20BIT,
  LTR390_RESOLUTION_19BIT,
  LTR390_RESOLUTION_18BIT,  // Default
  LTR390_RESOLUTION_17BIT,
  LTR390_RESOLUTION_16BIT,
  LTR390_RESOLUTION_13BIT,
};

class LTR390Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(light)
  SUB_SENSOR(als)
  SUB_SENSOR(uvi)
  SUB_SENSOR(uv)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_als_gain_value(LTR390GAIN gain) { this->gain_als_ = gain; }
  void set_uv_gain_value(LTR390GAIN gain) { this->gain_uv_ = gain; }
  void set_als_res_value(LTR390RESOLUTION res) { this->res_als_ = res; }
  void set_uv_res_value(LTR390RESOLUTION res) { this->res_uv_ = res; }
  void set_wfac_value(float wfac) { this->wfac_ = wfac; }

 protected:
  static constexpr uint8_t ENABLED_MODE_ALS = 1 << 0;
  static constexpr uint8_t ENABLED_MODE_UVS = 1 << 1;

  optional<uint32_t> read_sensor_data_(LTR390MODE mode);

  void read_als_();
  void read_uvs_();

  void read_mode_(LTR390MODE mode);
  void standby_();

  float wfac_;

  LTR390GAIN gain_als_;
  LTR390GAIN gain_uv_;
  LTR390RESOLUTION res_als_;
  LTR390RESOLUTION res_uv_;

  bool reading_{false};
  uint8_t enabled_modes_{0};
};

}  // namespace esphome::ltr390
