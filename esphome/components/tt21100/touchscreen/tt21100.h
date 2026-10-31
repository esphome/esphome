#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#ifdef TT21100_BUTTON_COUNT
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/helpers.h"

#include <utility>
#endif

namespace esphome::tt21100 {

using namespace touchscreen;

class TT21100Touchscreen final : public Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

#ifdef TT21100_BUTTON_COUNT
  /// Kept sorted by index (config order within an index) so a report publishes its keys in index order.
  void add_button_binary_sensor(binary_sensor::BinarySensor *sensor, uint8_t index) {
    this->buttons_.push_back({sensor, index});
    for (size_t i = this->buttons_.size() - 1; i > 0 && this->buttons_[i - 1].index > index; i--)
      std::swap(this->buttons_[i - 1], this->buttons_[i]);
  }
#endif

 protected:
  void update_touches() override;

  InternalGPIOPin *interrupt_pin_;
  GPIOPin *reset_pin_{nullptr};

#ifdef TT21100_BUTTON_COUNT
  struct ButtonSensor {
    binary_sensor::BinarySensor *sensor;
    uint8_t index;
  };
  StaticVector<ButtonSensor, TT21100_BUTTON_COUNT> buttons_;
#endif
};

}  // namespace esphome::tt21100
