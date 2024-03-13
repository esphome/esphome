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

class XPT2046Component : public Touchscreen,
                         public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                               spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_2MHZ> {
 public:
  /// Set the threshold for the touch detection.
  void set_threshold(int16_t threshold) { this->threshold_ = threshold; }
  /// Set the pin used to detect the touch.
  void set_irq_pin(InternalGPIOPin *pin) { this->irq_pin_ = pin; }

  void setup() override;
  void dump_config() override;
  // float get_setup_priority() const override;

 protected:
  static int16_t best_two_avg(int16_t value1, int16_t value2, int16_t value3);

  int16_t read_adc_(uint8_t ctrl);

  void update_touches() override;

  int16_t threshold_;

  InternalGPIOPin *irq_pin_{nullptr};
};

}  // namespace xpt2046
}  // namespace esphome
