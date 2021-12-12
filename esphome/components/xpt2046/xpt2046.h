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
  /// Set the touch screen area where the button will detect the touch.
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
  /// Set the logical touch screen dimensions.
  void set_dimensions(int16_t x, int16_t y) {
    this->x_dim_ = x;
    this->y_dim_ = y;
  }
  /// Set the coordinates for the touch screen edges.
  void set_calibration(int16_t x_min, int16_t x_max, int16_t y_min, int16_t y_max);
  /// If true the x and y axes will be swapped
  void set_swap_x_y(bool val) { this->swap_x_y_ = val; }

  /// Set the interval to report the touch point perodically.
  void set_report_interval(uint32_t interval) { this->report_millis_ = interval; }
  /// Set the threshold for the touch detection.
  void set_threshold(int16_t threshold) { this->threshold_ = threshold; }
  /// Set the pin used to detect the touch.
  void set_irq_pin(GPIOPin *pin) { this->irq_pin_ = pin; }
  /// Get an access to the on_state automation trigger
  XPT2046OnStateTrigger *get_on_state_trigger() const { return this->on_state_trigger_; }
  /// Register a virtual button to the component.
  void register_button(XPT2046Button *button) { this->buttons_.push_back(button); }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  /** Detect the touch if the irq pin is specified.
   *
   * If the touch is detected and the component does not already know about it
   * the update() is called immediately. If the irq pin is not specified
   * the loop() is a no-op.
   */
  void loop() override;

  /** Read and process the values from the hardware.
   *
   * Read the raw x, y and touch pressure values from the chip, detect the touch,
   * and if touched, transform to the user x and y coordinates. If the state has
   * changed or if the value should be reported again due to the
   * report interval, run the action and inform the virtual buttons.
   */
  void update() override;

  /**@{*/
  /** Coordinates of the touch position.
   *
   * The values are set immediately before the on_state action with touched == true
   * is triggered. The action with touched == false sends the coordinates of the last
   * reported touch.
   */
  int16_t x{0}, y{0};
  /**@}*/

  /// True if the component currently detects the touch
  bool touched{false};

  /**@{*/
  /** Raw sensor values of the coordinates and the pressure.
   *
   * The values are set each time the update() method is called.
   */
  int16_t x_raw{0}, y_raw{0}, z_raw{0};
  /**@}*/

 protected:
  static int16_t best_two_avg(int16_t x, int16_t y, int16_t z);
  static int16_t normalize(int16_t val, int16_t min_val, int16_t max_val);

  int16_t read_adc_(uint8_t ctrl);

  int16_t threshold_;
  int16_t x_raw_min_, x_raw_max_, y_raw_min_, y_raw_max_;
  int16_t x_dim_, y_dim_;
  bool invert_x_, invert_y_;
  bool swap_x_y_;

  uint32_t report_millis_;
  uint32_t last_pos_ms_{0};

  GPIOPin *irq_pin_{nullptr};
  bool last_irq_{true};

  XPT2046OnStateTrigger *on_state_trigger_{new XPT2046OnStateTrigger()};
  std::vector<XPT2046Button *> buttons_{};
};

}  // namespace xpt2046
}  // namespace esphome
