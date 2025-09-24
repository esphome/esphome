#include "mcp47a1.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp47a1 {

static const char *const TAG = "mcp47a1";

void MCP47A1::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP47A1 Output:");
  LOG_I2C_DEVICE(this);
}

void MCP47A1::write_state(float state) {
  const uint8_t value = remap(state, 0.0f, 1.0f, 63, 0);
  this->write_byte(0, value);
}

}  // namespace mcp47a1
}  // namespace esphome
