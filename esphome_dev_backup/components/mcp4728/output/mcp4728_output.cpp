#include "mcp4728_output.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp4728 {

void MCP4728Channel::write_state(float state) {
  const uint16_t max_duty = 4095;
  const float duty_rounded = roundf(state * max_duty);
  auto duty = static_cast<uint16_t>(duty_rounded);
  this->parent_->set_channel_value_(this->channel_, duty);
}

}  // namespace mcp4728
}  // namespace esphome
