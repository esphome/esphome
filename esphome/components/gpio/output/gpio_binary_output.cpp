#include "gpio_binary_output.h"
#include "esphome/core/log.h"

namespace esphome::gpio {

ESPHOME_LOG_TAG(TAG, "gpio.output");

void GPIOBinaryOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Binary Output:");
  LOG_PIN("  Pin: ", this->pin_);
  LOG_BINARY_OUTPUT(this);
}

}  // namespace esphome::gpio
