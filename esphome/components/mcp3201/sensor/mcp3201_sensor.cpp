#include "mcp3201_sensor.h"

#include "esphome/core/log.h"

namespace esphome {
namespace mcp3201 {

static const char *const TAG = "mcp3201.sensor";

float MCP3201Sensor::get_setup_priority() const { return setup_priority::DATA; }

void MCP3201Sensor::dump_config() {
  LOG_SENSOR("", "MCP3201 Sensor", this);
  LOG_UPDATE_INTERVAL(this);
}
float MCP3201Sensor::sample() { return this->parent_->read_data(); }
void MCP3201Sensor::update() { this->publish_state(this->sample()); }

}  // namespace mcp3201
}  // namespace esphome
