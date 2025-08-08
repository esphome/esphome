#pragma once

#include "esphome/components/output/float_output.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#if defined(USE_ESP32_VARIANT_ESP32) || defined(USE_ESP32_VARIANT_ESP32S2)

#include <driver/dac_oneshot.h>

namespace esphome {
namespace esp32_dac {

class ESP32DAC : public output::FloatOutput, public Component {
 public:
  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }

  /// Initialize pin
  void setup() override;
  void on_safe_shutdown() override;
  void dump_config() override;
  /// HARDWARE setup_priority
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override;

  InternalGPIOPin *pin_;
  dac_oneshot_handle_t dac_handle_;
};

}  // namespace esp32_dac
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32 || USE_ESP32_VARIANT_ESP32S2
