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

// https://github.com/wigman27/Tutorial-Using-Arduino-SPI/blob/master/Arduino_MCP3204_ADC.ino
float MCP3204::read_data(uint8_t pin) {
  byte adcPrimaryConfig = 0b00000110 & 0b00000111; // ensures the ADC register is limited to the mask and assembles the configuration byte to send to ADC.
  byte adcSecondaryConfig = pin << 6; // shift the bits to get a binary number
  this->enable();
  this->transfer_byte(adcPrimaryConfig); //  send in the primary configuration address byte -- we don't need a return
  byte adcPrimaryByte = this->transfer_byte(adcSecondaryConfig); // read the primary byte, also sending in the secondary address byte.
  byte adcSecondaryByte = this->transfer_byte(0x00); // read the secondary byte, also sending 0 as this doesn't matter.
  this->disable();
  uint16_t digitalValue = ( adcPrimaryByte << 8 | adcSecondaryByte ) & 0b111111111111; //cast to keep only our 12 bits
  return float(digitalValue) / 4096.000;   // 4096 is the max, so divide according to the datasheet
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
