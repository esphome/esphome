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
  uint8_t command;

  command = (1 << 6) |                       // start bit
            ((differential ? 0 : 1) << 5) |  // single or differential bit
            ((pin & 0x07) << 2);             // pin

  // A single transfer_array() is one SPI transaction; three transfer_byte()
  // calls are three. On ESP-IDF each one is its own
  // spi_device_polling_start/_end cycle, and that per-transaction overhead
  // dwarfs the 24 bits of payload - it directly caps how often a
  // voltage_sampler consumer such as ct_clamp can sample. The bits on the
  // wire are unchanged: CS was already held low across all three bytes.
  uint8_t buffer[3] = {command, 0x00, 0x00};

  this->enable();
  this->transfer_array(buffer, sizeof(buffer));
  this->disable();

  uint16_t digital_value = encode_uint16(buffer[1], buffer[2]) >> 4;
  return float(digital_value) / 4096.000f * this->reference_voltage_;  // in V
}

}  // namespace esphome::mcp3204
