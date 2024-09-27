#include "mcp3204.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp3204 {

static const char *const TAG = "mcp3204";

float MCP3204::get_setup_priority() const { return setup_priority::HARDWARE; }

void MCP3204::setup() {
  ESP_LOGCONFIG(TAG, "Setting up mcp3204");
  this->spi_setup();
}

void MCP3204::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP3204:");
  LOG_PIN("  CS Pin:", this->cs_);
  ESP_LOGCONFIG(TAG, "  Reference Voltage: %.2fV", this->reference_voltage_);
}

float MCP3204::read_data(uint8_t channel, bool differential) {
  uint8_t command, sgldiff;
  uint8_t b0, b1, b2;

  sgldiff = differential ? 0 : 1;

  command = ((0x01 << 7) |              // start bit
             (sgldiff << 6) |           // single or differential
             ((channel & 0x07) << 3));  // channel number

  this->enable();
  b0 = this->transfer_byte(command);
  b1 = this->transfer_byte(0x00);
  b2 = this->transfer_byte(0x00);
  this->disable();

  uint16_t digital_value = 0xFFF & ((b0 & 0x01) << 11 | (b1 & 0xFF) << 3 | (b2 & 0xE0) >> 5);
  return float(digital_value) / 4096.000 * this->reference_voltage_;  // in V
}

}  // namespace mcp3204
}  // namespace esphome
