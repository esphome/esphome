#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/core/automation.h"
#include "esphome/components/output/float_output.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace esp32_dac {

class ESP32DAC : public output::FloatOutput, public Component {
 public:
  void set_pin(GPIOPin *pin) { pin_ = pin; }

  /// Initialize pin
  void setup() override;
  void dump_config() override;
  /// HARDWARE setup_priority
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override;

  GPIOPin *pin_;
};

}  // namespace esp32_dac
}  // namespace esphome

#endif
