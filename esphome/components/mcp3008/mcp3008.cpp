#include "mcp3008.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp3008 {

static const char *TAG = "mcp3008";

float MCP3008::get_setup_priority() const { return setup_priority::HARDWARE; }

void MCP3008::setup() {
  ESP_LOGCONFIG(TAG, "Setting up mcp3008");
  this->spi_setup();
  ESP_LOGI(TAG, "SPI setup finished!");
}

void MCP3008::dump_config() { LOG_PIN("  CS Pin: ", this->cs_); }

float MCP3008::readData(uint8_t pin) {
  byte b0, b1, b2;

  byte command = ((0x01 << 7) |          // start bit
                  (0 << 6) |             // single or differential
                  ((pin & 0x07) << 3));  // channel number

  this->enable();

  b0 = this->transfer_byte(command);
  b1 = this->transfer_byte(0x00);
  b2 = this->transfer_byte(0x00);

  this->disable();

  int data = 0x3FF & ((b0 & 0x01) << 9 | (b1 & 0xFF) << 1 | (b2 & 0x80) >> 7);

  return data / 1024.0f;
}

MCP3008Sensor::MCP3008Sensor(MCP3008 *parent, std::string name, uint8_t pin)
    : PollingComponent(1000), parent_(parent), _pin(pin) {
  this->set_name(name);
}
void MCP3008Sensor::setup() { ESP_LOGCONFIG(TAG, "Setting up MCP3008 Sensor '%s'...", this->get_name().c_str()); }
void MCP3008Sensor::update() {
  float value_v = this->parent_->readData(_pin);
  this->publish_state(value_v);
}

}  // namespace mcp3008
}  // namespace esphome
