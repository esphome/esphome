#include "mcp3201.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp3201 {

static const char *const TAG = "mcp3201";

float MCP3201::get_setup_priority() const { return setup_priority::HARDWARE; }

void MCP3201::setup() { this->spi_setup(); }

void MCP3201::dump_config() {
  ESP_LOGCONFIG(TAG,
                "MCP3201:\n"
                "  Reference Voltage: %.2fV",
                this->reference_voltage_);
  LOG_PIN("  CS Pin:", this->cs_);
}

float MCP3201::read_data() {
  uint8_t b0, b1;

  this->enable();
  b0 = this->transfer_byte(0x00);
  b1 = this->transfer_byte(0x00);
  this->disable();

  // MCP3201: First 3 bits of b0 are null, then 12-bit value spans b0[4:0] and b1[7:1]
  // Bit pattern: b0 = xxxBBBBB, b1 = BBBBBBBy (where x=null, B=data bit, y=unused LSB)
  uint16_t digital_value = ((b0 & 0x1F) << 7) | (b1 >> 1);
  return float(digital_value) / 4096.000 * this->reference_voltage_;  // in V
}

}  // namespace mcp3201
}  // namespace esphome
