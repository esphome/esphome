#include "mcp3008.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp3008 {

static const char *TAG = "mcp3008";

float MCP3008::get_setup_priority() const { return setup_priority::HARDWARE; }

void MCP3008::setup() {
  ESP_LOGCONFIG(TAG, "Setting up mcp3008");
  this->spi_setup();
}

void MCP3008::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP3008:");
  LOG_PIN("  CS Pin: ", this->cs_);
}

float MCP3008::read_data_(uint8_t pin) {
  uint8_t data_msb = 0;
  uint8_t data_lsb = 0;

  uint8_t command = ((0x01 << 7) |          // start bit
                     ((pin & 0x07) << 4));  // channel number

  this->enable();

  this->transfer_byte(0x01);
  data_msb = this->transfer_byte(command) & 0x03;
  data_lsb = this->transfer_byte(0x00);

  this->disable();

  int data = data_msb << 8 | data_lsb;

  return data / 1024.0f;
}

MCP3008Sensor::MCP3008Sensor(MCP3008 *parent, std::string name, uint8_t pin)
    : PollingComponent(1000), parent_(parent), pin_(pin) {
  this->set_name(name);
}
void MCP3008Sensor::setup() { LOG_SENSOR("", "Setting up MCP3008 Sensor '%s'...", this); }
void MCP3008Sensor::update() {
  float value_v = this->parent_->read_data_(pin_);
  this->publish_state(value_v);
}

}  // namespace mcp3008
}  // namespace esphome
