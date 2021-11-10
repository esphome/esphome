#include "mcp3008.h"
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

  int data = data_msb << 8 | data_lsb;

  return data / 1023.0f;
}

MCP3008Sensor::MCP3008Sensor(MCP3008 *parent, uint8_t pin, float reference_voltage)
    : PollingComponent(1000), parent_(parent), pin_(pin), reference_voltage_(reference_voltage) {}

float MCP3008Sensor::get_setup_priority() const { return setup_priority::DATA; }

void MCP3008Sensor::setup() { LOG_SENSOR("", "Setting up MCP3008 Sensor '%s'...", this); }
void MCP3008Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP3008Sensor:");
  ESP_LOGCONFIG(TAG, "  Pin: %u", this->pin_);
  ESP_LOGCONFIG(TAG, "  Reference Voltage: %.2fV", this->reference_voltage_);
}
float MCP3008Sensor::sample() {
  float value_v = this->parent_->read_data(pin_);
  value_v = (value_v * this->reference_voltage_);
  return value_v;
}
void MCP3008Sensor::update() { this->publish_state(this->sample()); }

}  // namespace mcp3008
}  // namespace esphome
