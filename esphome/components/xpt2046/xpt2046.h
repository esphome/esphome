#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace xpt2046 {

enum XPT2046Transform {
  SWAP_X_Y,
  INVERT_X,
  INVERT_Y,
};

class XPT2046Component : public PollingComponent,
                         public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                               spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_2MHZ> {
 public:
  void set_touched_sensor(sensor::Sensor *touched_sensor) { this->touched_sensor_ = touched_sensor; }
  void set_x_sensor(sensor::Sensor *x_sensor) { this->x_sensor_ = x_sensor; }
  void set_y_sensor(sensor::Sensor *y_sensor) { this->y_sensor_ = y_sensor; }
  void set_tirq_pin(GPIOPin *tirq) { this->tirq_pin_ = tirq; }
  void set_transform(XPT2046Transform trans) { this->transform_ |= (1U << (int) trans); }

  void set_dimensions(int16_t x, int16_t y) {
    this->x_dim_ = x;
    this->y_dim_ = y;
  }
  void set_calibration(int16_t x_min, int16_t x_max, int16_t y_min, int16_t y_max) {
    this->x_raw_min_ = x_min;
    this->x_raw_max_ = x_max;
    this->y_raw_min_ = y_min;
    this->y_raw_max_ = y_max;
  }

  void set_report_interval(uint16_t interval) { report_millis_ = interval; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

 protected:
  static int16_t best_two_avg(int16_t x, int16_t y, int16_t z);
  static int16_t normalize(int16_t val, int16_t min_val, int16_t max_val);

  int16_t read_adc_(uint8_t ctrl);

  sensor::Sensor *touched_sensor_;
  sensor::Sensor *x_sensor_;
  sensor::Sensor *y_sensor_;

  GPIOPin *tirq_pin_ = nullptr;

  uint8_t transform_ = 0;

  int16_t x_raw_ = 0, y_raw_ = 0, z_raw_ = 0;
  int16_t x_out_ = 0, y_out_ = 0, touched_out_ = 0;

  int16_t x_raw_min_ = 0, x_raw_max_ = 4095, y_raw_min_ = 0, y_raw_max_ = 4095;
  int16_t x_dim_ = 100, y_dim_ = 100;

  uint16_t report_millis_ = 1000;
  unsigned long last_pos_ms_;
};

}  // namespace xpt2046
}  // namespace esphome
