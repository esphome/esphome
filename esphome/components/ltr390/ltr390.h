#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include <map>
#include <atomic>
#include <tuple>

namespace esphome {
namespace ltr390 {

typedef enum {
  LTR390_CTRL_EN = 1,
  LTR390_CTRL_MODE = 3,
  LTR390_CTRL_RST = 4,
} ltr390_ctrl_t;

// enums from https://github.com/adafruit/Adafruit_LTR390/

#define LTR390_MAIN_CTRL 0x00       ///< Main control register
#define LTR390_MEAS_RATE 0x04       ///< Resolution and data rate
#define LTR390_GAIN 0x05            ///< ALS and UVS gain range
#define LTR390_PART_ID 0x06         ///< Part id/revision register
#define LTR390_MAIN_STATUS 0x07     ///< Main status register

#define LTR390_SENSITIVITY 2300.0

// Sensing modes
typedef enum {
  LTR390_MODE_ALS,
  LTR390_MODE_UVS,
} ltr390_mode_t;

// Sensor gain levels
typedef enum {
  LTR390_GAIN_1 = 0,
  LTR390_GAIN_3, // Default
  LTR390_GAIN_6,
  LTR390_GAIN_9,
  LTR390_GAIN_18,
} ltr390_gain_t;

// Sensor resolution
typedef enum {
  LTR390_RESOLUTION_20BIT,
  LTR390_RESOLUTION_19BIT,
  LTR390_RESOLUTION_18BIT, // Default
  LTR390_RESOLUTION_17BIT,
  LTR390_RESOLUTION_16BIT,
  LTR390_RESOLUTION_13BIT,
} ltr390_resolution_t;

class LTR390Component : public PollingComponent, public i2c::I2CDevice {
  public:
    void setup() override;
    void dump_config() override;
    void update() override;
    float get_setup_priority() const override { return setup_priority::DATA; }

    void set_gain_value(ltr390_gain_t gain) { this->gain_ = gain; }
    void set_res_value(ltr390_resolution_t res) { this->res_ = res; }
    void set_wfac_value(float wfac) { this->wfac_ = wfac; }

    void set_light_sensor(sensor::Sensor *light_sensor) { this->light_sensor_ = light_sensor; }
    void set_als_sensor(sensor::Sensor *als_sensor) { this->als_sensor_ = als_sensor; }
    void set_uvi_sensor(sensor::Sensor *uvi_sensor) { this->uvi_sensor_ = uvi_sensor; }
    void set_uv_sensor(sensor::Sensor *uv_sensor) { this->uv_sensor_ = uv_sensor; }

  protected:
    bool enabled();
    void enable(bool en);

    bool reset(void);

    void set_mode(ltr390_mode_t mode);
    ltr390_mode_t get_mode(void);

    void set_gain(ltr390_gain_t gain);
    ltr390_gain_t get_gain(void);

    void set_resolution(ltr390_resolution_t res);
    ltr390_resolution_t get_resolution(void);

    bool new_data_available(void);
    uint32_t read_sensor_data(ltr390_mode_t mode);

    void read_als(void);
    void read_uvs(void);

    void read_mode(int mode_index);

    std::atomic<bool> reading;

    std::vector< std::tuple< ltr390_mode_t, std::function<void(void)> > > *mode_funcs_;

    i2c::I2CRegister *ctrl_reg_;
    i2c::I2CRegister *status_reg_;
    i2c::I2CRegister *gain_reg_;
    i2c::I2CRegister *res_reg_;

    ltr390_gain_t gain_;
    ltr390_resolution_t res_;
    float wfac_;

    sensor::Sensor *light_sensor_{nullptr};
    sensor::Sensor *als_sensor_{nullptr};

    sensor::Sensor *uvi_sensor_{nullptr};
    sensor::Sensor *uv_sensor_{nullptr};

};

}  // namespace ltr390
}  // namespace esphome
