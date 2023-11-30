#include "mcp3008.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp3008 {

static const char *const TAG = "mcp3008";

float MCP3008::get_setup_priority() const { return setup_priority::HARDWARE; }

void MCP3008::setup() {
  ESP_LOGCONFIG(TAG, "Setting up mcp3008");
  this->spi_setup();
}

void MCP3008::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP3008:");
  LOG_PIN("  CS Pin:", this->cs_);
}

float MCP3008::read_data(uint8_t pin) {
  uint8_t data_msb, data_lsb = 0;

  uint8_t command = ((0x01 << 7) |          // start bit
                     ((pin & 0x07) << 4));  // channel number

  this->enable();
  this->transfer_byte(0x01);

  data_msb = this->transfer_byte(command) & 0x03;
  data_lsb = this->transfer_byte(0x00);

  this->disable();

  uint16_t data = encode_uint16(data_msb, data_lsb);

  return data / 1023.0f;
}

}  // namespace mcp3008
}  // namespace esphome
