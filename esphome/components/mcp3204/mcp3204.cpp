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

float MCP3204::read_data(uint8_t pin) {
  uint8_t adc_primary_config = 0b00000110 | (pin >> 2);
  uint8_t adc_secondary_config = pin << 6;
  this->enable();
  this->transfer_byte(adc_primary_config);
  uint8_t adc_primary_byte = this->transfer_byte(adc_secondary_config);
  uint8_t adc_secondary_byte = this->transfer_byte(0x00);
  this->disable();
  uint16_t digital_value = (adc_primary_byte << 8 | adc_secondary_byte) & 0b111111111111;
  return float(digital_value) / 4096.000 * this->reference_voltage_;
}

}  // namespace mcp3204
}  // namespace esphome
