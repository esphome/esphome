#include "mcp3221_sensor.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::mcp3221 {

ESPHOME_LOG_TAG(TAG, "mcp3221");

float MCP3221Sensor::sample() {
  uint8_t data[2];
  if (!this->status_comm_result(this->read(data, 2) == i2c::ERROR_OK)) {
    ESP_LOGW(TAG, "Read failed");
    return NAN;
  }

  uint16_t value = encode_uint16(data[0], data[1]);
  float voltage = value * this->reference_voltage_ / 4096.0f;

  return voltage;
}

void MCP3221Sensor::update() {
  float v = this->sample();
  this->publish_state(v);
}

}  // namespace esphome::mcp3221
