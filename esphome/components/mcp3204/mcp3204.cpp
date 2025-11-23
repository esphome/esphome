#include "mcp3204.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp3204 {

static const char *const TAG = "mcp3204";

float MCP3204::get_setup_priority() const { return setup_priority::HARDWARE; }

void MCP3204::setup() { this->spi_setup(); }

void MCP3204::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP3204:");
  LOG_PIN("  CS Pin:", this->cs_);
  ESP_LOGCONFIG(TAG, "  Reference Voltage: %.2fV", this->reference_voltage_);
}

float MCP3204::read_data(uint8_t pin, bool differential) {
  uint8_t command, b0, b1;

  command = (1 << 6) |                       // start bit
            ((differential ? 0 : 1) << 5) |  // single or differential bit
            ((pin & 0x07) << 2);             // pin

  this->enable();
  this->transfer_byte(command);
  b0 = this->transfer_byte(0x00);
  b1 = this->transfer_byte(0x00);
  this->disable();

  uint16_t digital_value = encode_uint16(b0, b1) >> 4;
  return float(digital_value) / 4096.000 * this->reference_voltage_;  // in V
}

}  // namespace mcp3204
}  // namespace esphome
