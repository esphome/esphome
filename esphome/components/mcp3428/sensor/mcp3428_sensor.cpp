#include "mcp3428_sensor.h"

#include "esphome/core/log.h"

namespace esphome {
namespace mcp3428 {

static const char *const TAG = "mcp3426/7/8.sensor";

float MCP3428Sensor::sample() {
  return this->parent_->request_measurement(this->multiplexer_, this->gain_, this->resolution_);
}

void MCP3428Sensor::update() {
  float v = this->sample();
  if (!std::isnan(v)) {
    ESP_LOGD(TAG, "'%s': Got Voltage=%fV", this->get_name().c_str(), v);
    this->publish_state(v);
  }
}

void MCP3428Sensor::dump_config() {
  LOG_SENSOR("  ", "MCP3426/7/8 Sensor", this);
  ESP_LOGCONFIG(TAG, "    Multiplexer: %u", this->multiplexer_);
  ESP_LOGCONFIG(TAG, "    Gain: %u", this->gain_);
  ESP_LOGCONFIG(TAG, "    Resolution: %u", this->resolution_);
}

}  // namespace mcp3428
}  // namespace esphome
