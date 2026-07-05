#pragma once

#ifdef USE_ZEPHYR

#include "esphome/components/output/float_output.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include <zephyr/device.h>

namespace esphome::zephyr_pwm {

class ZephyrPWMChannel : public output::FloatOutput, public Component {
 public:
  explicit ZephyrPWMChannel(const struct device *device, uint8_t channel, bool inverted, uint32_t period_ns)
      : device_(device), channel_(channel), inverted_(inverted), period_ns_(period_ns) {}

  void setup() override;
  void dump_config() override;
  /// HARDWARE setup_priority
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override;

  const struct device *device_;
  uint8_t channel_;
  bool inverted_;
  uint32_t period_ns_;
};
}  // namespace esphome::zephyr_pwm

#endif  // USE_ZEPHYR
