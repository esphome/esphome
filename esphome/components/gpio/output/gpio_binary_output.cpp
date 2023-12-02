#include "gpio_binary_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gpio {

static const char *const TAG = "gpio.output";

void GPIOBinaryOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "GPIO Binary Output:");
  LOG_PIN("  Pin: ", this->pin_);
  LOG_BINARY_OUTPUT(this);
}

}  // namespace gpio
}  // namespace esphome
