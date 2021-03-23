#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace xpt2046 {

class XPT2046OnStateTrigger : public Trigger<int, int, bool> {
 public:
  void process(int x, int y, bool touched);
};

class XPT2046Button : public binary_sensor::BinarySensor {
 public:
  void set_area(int16_t x_min, int16_t x_max, int16_t y_min, int16_t y_max) {
    this->x_min_ = x_min;
    this->x_max_ = x_max;
    this->y_min_ = y_min;
    this->y_max_ = y_max;
  }

  void touch(int16_t x, int16_t y);
  void release();

 protected:
  int16_t x_min_, x_max_, y_min_, y_max_;
  bool state_{false};
};

class XPT2046Component : public PollingComponent,
                         public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                               spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_2MHZ> {
 public:
  void set_dimensions(int16_t x, int16_t y) {
    this->x_dim_ = x;
    this->y_dim_ = y;
  }
  void set_calibration(int16_t x_min, int16_t x_max, int16_t y_min, int16_t y_max);
  void set_swap_x_y(bool val) { this->swap_x_y_ = val; }

  void set_report_interval(uint16_t interval) { this->report_millis_ = interval; }
  void set_threshold(int16_t threshold) { this->threshold_ = threshold; }

  XPT2046OnStateTrigger *get_on_state_trigger() const { return this->on_state_trigger_; }
  void register_button(XPT2046Button *button) { this->buttons_.push_back(button); }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

 protected:
  static int16_t best_two_avg(int16_t x, int16_t y, int16_t z);
  static int16_t normalize(int16_t val, int16_t min_val, int16_t max_val);

  int16_t read_adc_(uint8_t ctrl);

  uint8_t transform_{0};

  int16_t x_raw_{0}, y_raw_{0}, z_raw_{0};
  int16_t x_out_{0}, y_out_{0};
  bool touched_out_{false};

  int16_t threshold_;
  int16_t x_raw_min_, x_raw_max_, y_raw_min_, y_raw_max_;
  int16_t x_dim_, y_dim_;
  bool invert_x_, invert_y_;
  bool swap_x_y_;

  uint16_t report_millis_;
  unsigned long last_pos_ms_{0};

  XPT2046OnStateTrigger *on_state_trigger_{new XPT2046OnStateTrigger()};
  std::vector<XPT2046Button *> buttons_{};
};

}  // namespace xpt2046
}  // namespace esphome
