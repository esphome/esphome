#include "mcp4725.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp4725 {

static const char *const TAG = "mcp4725";

void MCP4725::setup() {
  auto err = this->write(nullptr, 0);
  if (err != i2c::ERROR_OK) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
}

void MCP4725::dump_config() {
  LOG_I2C_DEVICE(this);

  if (this->error_code_ == COMMUNICATION_FAILED) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

// https://learn.sparkfun.com/tutorials/mcp4725-digital-to-analog-converter-hookup-guide?_ga=2.176055202.1402343014.1607953301-893095255.1606753886
void MCP4725::write_state(float state) {
  const uint16_t value = (uint16_t) round(state * (pow(2, MCP4725_RES) - 1));

  this->write_byte_16(64, value << 4);
}

}  // namespace mcp4725
}  // namespace esphome
