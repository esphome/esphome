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
}

float MCP3204::read_data(uint8_t pin) {
  uint8_t adc_primary_config = 0b00000110 & 0b00000111;
  uint8_t adc_secondary_config = pin << 6;
  this->enable();
  this->transfer_byte(adc_primary_config);
  uint8_t adc_primary_byte = this->transfer_byte(adc_secondary_config);
  uint8_t adc_secondary_byte = this->transfer_byte(0x00);
  this->disable();
  uint16_t digital_value = (adc_primary_byte << 8 | adc_secondary_byte) & 0b111111111111;
  return float(digital_value) / 4096.000;
}

MCP3204Sensor::MCP3204Sensor(MCP3204 *parent, uint8_t pin, float reference_voltage)
    : PollingComponent(1000), parent_(parent), pin_(pin), reference_voltage_(reference_voltage) {}

float MCP3204Sensor::get_setup_priority() const { return setup_priority::DATA; }

void MCP3204Sensor::setup() { LOG_SENSOR("", "Setting up MCP3204 Sensor '%s'...", this); }
void MCP3204Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP3204Sensor:");
  ESP_LOGCONFIG(TAG, "  Pin: %u", this->pin_);
  ESP_LOGCONFIG(TAG, "  Reference Voltage: %.2fV", this->reference_voltage_);
}
float MCP3204Sensor::sample() {
  float value_v = this->parent_->read_data(pin_);
  value_v = (value_v * this->reference_voltage_);
  return value_v;
}
void MCP3204Sensor::update() { this->publish_state(this->sample()); }

}  // namespace mcp3204
}  // namespace esphome
