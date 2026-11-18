#include "mcp4725.h"
#include "esphome/core/log.h"

namespace esphome::mcp4725 {

ESPHOME_LOG_TAG(TAG, "mcp4725");

void MCP4725::setup() {
  auto err = this->write(nullptr, 0);
  if (err != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
}

void MCP4725::dump_config() { LOG_I2C_DEVICE(this); }

// https://learn.sparkfun.com/tutorials/mcp4725-digital-to-analog-converter-hookup-guide?_ga=2.176055202.1402343014.1607953301-893095255.1606753886
void MCP4725::write_state(float state) {
  constexpr uint16_t max_value = (1U << MCP4725_RES) - 1;
  const uint16_t value = (uint16_t) roundf(state * max_value);

  this->write_byte_16(64, value << 4);
}

}  // namespace esphome::mcp4725
