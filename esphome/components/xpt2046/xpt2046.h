#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace xpt2046 {

using namespace touchscreen;

struct XPT2046TouchscreenStore {
  volatile bool touch;
  static void gpio_intr(XPT2046TouchscreenStore *store);
};

class XPT2046Component : public Touchscreen,
                         public PollingComponent,
                         public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                               spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_2MHZ> {
 public:
  /// Set the logical touch screen dimensions.
  void set_dimensions(int16_t x, int16_t y) {
    this->display_width_ = x;
    this->display_height_ = y;
  }
  /// Set the coordinates for the touch screen edges.
  void set_calibration(int16_t x_min, int16_t x_max, int16_t y_min, int16_t y_max);
  /// If true the x and y axes will be swapped
  void set_swap_x_y(bool val) { this->swap_x_y_ = val; }

  /// Set the interval to report the touch point perodically.
  void set_report_interval(uint32_t interval) { this->report_millis_ = interval; }
  uint32_t get_report_interval() { return this->report_millis_; }

  /// Set the threshold for the touch detection.
  void set_threshold(int16_t threshold) { this->threshold_ = threshold; }
  /// Set the pin used to detect the touch.
  void set_irq_pin(InternalGPIOPin *pin) { this->irq_pin_ = pin; }

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
  void check_touch_();

  int16_t threshold_;
  int16_t x_raw_min_, x_raw_max_, y_raw_min_, y_raw_max_;

  bool invert_x_, invert_y_;
  bool swap_x_y_;

  uint32_t report_millis_;
  uint32_t last_pos_ms_{0};

  InternalGPIOPin *irq_pin_{nullptr};
  XPT2046TouchscreenStore store_;
};

}  // namespace xpt2046
}  // namespace esphome
