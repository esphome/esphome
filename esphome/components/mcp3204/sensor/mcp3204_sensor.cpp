#include "mcp3204_sensor.h"

#include "esphome/core/log.h"

namespace esphome {
namespace mcp3204 {

static const char *const TAG = "mcp3204.sensor";

MCP3204Sensor::MCP3204Sensor(uint8_t pin, float reference_voltage) : pin_(pin), reference_voltage_(reference_voltage) {}

float MCP3204Sensor::get_setup_priority() const { return setup_priority::DATA; }

void MCP3204Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP3204Sensor:");
  ESP_LOGCONFIG(TAG, "  Pin: %u", this->pin_);
  ESP_LOGCONFIG(TAG, "  Reference Voltage: %.2fV", this->reference_voltage_);
  LOG_UPDATE_INTERVAL(this);
}
float MCP3204Sensor::sample() {
  float value_v = this->parent_->read_data(this->pin_);
  value_v = (value_v * this->reference_voltage_);
  return value_v;
}
void MCP3204Sensor::update() { this->publish_state(this->sample()); }

}  // namespace mcp3204
}  // namespace esphome
