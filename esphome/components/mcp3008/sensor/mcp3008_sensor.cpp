#include "mcp3008_sensor.h"

#include "esphome/core/log.h"

namespace esphome {
namespace mcp3008 {

static const char *const TAG = "mcp3008.sensor";

float MCP3008Sensor::get_setup_priority() const { return setup_priority::DATA; }

void MCP3008Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP3008Sensor:");
  ESP_LOGCONFIG(TAG, "  Pin: %u", this->pin_);
  ESP_LOGCONFIG(TAG, "  Reference Voltage: %.2fV", this->reference_voltage_);
}

float MCP3008Sensor::sample() {
  float value_v = this->parent_->read_data(pin_);
  value_v = (value_v * this->reference_voltage_);
  return value_v;
}

void MCP3008Sensor::update() { this->publish_state(this->sample()); }

}  // namespace mcp3008
}  // namespace esphome
