#include "mcp3204.h"
#include "esphome/core/log.h"

namespace esphome::mcp3204 {

static const char *const TAG = "mcp3204";

float MCP3204::get_setup_priority() const { return setup_priority::HARDWARE; }

void MCP3204::setup() { this->spi_setup(); }

void MCP3204::dump_config() {
  ESP_LOGCONFIG(TAG,
                "MCP3204:\n"
                "  Reference Voltage: %.2fV",
                this->reference_voltage_);
  LOG_PIN("  CS Pin:", this->cs_);
}

float MCP3204::read_data(uint8_t pin, bool differential) {
  const uint8_t command = (1 << 6) |                       // start bit
                          ((differential ? 0 : 1) << 5) |  // single or differential bit
                          ((pin & 0x07) << 2);             // pin
  // One full-duplex transaction: command out, 12-bit result back in bytes 1 and 2.
  // Word aligned so ESP-IDF DMA uses the buffer in place; only ESP32-P4 also checks the length and bounces.
  alignas(4) uint8_t buffer[3] = {command, 0x00, 0x00};

  this->enable();
  this->transfer_array(buffer, sizeof(buffer));
  this->disable();

  uint16_t digital_value = encode_uint16(buffer[1], buffer[2]) >> 4;
  return float(digital_value) / 4096.000f * this->reference_voltage_;  // in V
}

}  // namespace esphome::mcp3204
