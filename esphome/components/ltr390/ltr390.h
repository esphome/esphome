#pragma once

#include <tuple>
#include <vector>
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/optional.h"

namespace esphome {
namespace ltr390 {

enum LTR390CTRL {
  LTR390_CTRL_EN = 1,
  LTR390_CTRL_MODE = 3,
  LTR390_CTRL_RST = 4,
};

// enums from https://github.com/adafruit/Adafruit_LTR390/

static const uint8_t LTR390_MAIN_CTRL = 0x00;
static const uint8_t LTR390_MEAS_RATE = 0x04;
static const uint8_t LTR390_GAIN = 0x05;
static const uint8_t LTR390_PART_ID = 0x06;
static const uint8_t LTR390_MAIN_STATUS = 0x07;
static const float LTR390_SENSITIVITY = 2300.0;

// Sensing modes
enum LTR390MODE {
  LTR390_MODE_ALS,
  LTR390_MODE_UVS,
};

// Sensor gain levels
enum LTR390GAIN {
  LTR390_GAIN_1 = 0,
  LTR390_GAIN_3,  // Default
  LTR390_GAIN_6,
  LTR390_GAIN_9,
  LTR390_GAIN_18,
};

// Sensor resolution
enum LTR390RESOLUTION {
  LTR390_RESOLUTION_20BIT,
  LTR390_RESOLUTION_19BIT,
  LTR390_RESOLUTION_18BIT,  // Default
  LTR390_RESOLUTION_17BIT,
  LTR390_RESOLUTION_16BIT,
  LTR390_RESOLUTION_13BIT,
};

class LTR390Component : public PollingComponent, public i2c::I2CDevice {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_gain_value(LTR390GAIN gain) { this->gain_ = gain; }
  void set_res_value(LTR390RESOLUTION res) { this->res_ = res; }
  void set_wfac_value(float wfac) { this->wfac_ = wfac; }

  void set_light_sensor(sensor::Sensor *light_sensor) { this->light_sensor_ = light_sensor; }
  void set_als_sensor(sensor::Sensor *als_sensor) { this->als_sensor_ = als_sensor; }
  void set_uvi_sensor(sensor::Sensor *uvi_sensor) { this->uvi_sensor_ = uvi_sensor; }
  void set_uv_sensor(sensor::Sensor *uv_sensor) { this->uv_sensor_ = uv_sensor; }

 protected:
  optional<uint32_t> read_sensor_data_(LTR390MODE mode);

  void read_als_();
  void read_uvs_();

  void read_mode_(int mode_index);

  bool reading_;

  // a list of modes and corresponding read functions
  std::vector<std::tuple<LTR390MODE, std::function<void()>>> mode_funcs_;

  LTR390GAIN gain_;
  LTR390RESOLUTION res_;
  float wfac_;

  sensor::Sensor *light_sensor_{nullptr};
  sensor::Sensor *als_sensor_{nullptr};

  sensor::Sensor *uvi_sensor_{nullptr};
  sensor::Sensor *uv_sensor_{nullptr};
};

}  // namespace ltr390
}  // namespace esphome
