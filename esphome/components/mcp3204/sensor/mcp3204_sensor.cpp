#include "mcp3204_sensor.h"

#include "esphome/core/log.h"

namespace esphome {
namespace mcp3204 {

static const char *const TAG = "mcp3204.sensor";

MCP3204Sensor::MCP3204Sensor(uint8_t pin) : pin_(pin) {}

float MCP3204Sensor::get_setup_priority() const { return setup_priority::DATA; }

void MCP3204Sensor::dump_config() {
  LOG_SENSOR("", "MCP3204 Sensor", this);
  ESP_LOGCONFIG(TAG, "  Pin: %u", this->pin_);
  LOG_UPDATE_INTERVAL(this);
}
float MCP3204Sensor::sample() { return this->parent_->read_data(this->pin_); }
void MCP3204Sensor::update() { this->publish_state(this->sample()); }

}  // namespace mcp3204
}  // namespace esphome
