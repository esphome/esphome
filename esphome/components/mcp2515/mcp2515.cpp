#include "mcp2515.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp2515 {

static const char *TAG = "mcp2515";

bool MCP2515::send_internal_(int can_id, uint8_t *data) { return true; };

}  // namespace mcp2515
}  // namespace esphome
