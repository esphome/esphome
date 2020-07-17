#include "mcp9808.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp9808 {

static const uint8_t MCP9808_REG_AMBIENT_TEMP = 0x05;
static const uint8_t MCP9808_REG_MANUF_ID = 0x06;
static const uint8_t MCP9808_REG_DEVICE_ID = 0x07;

static const char *TAG = "mcp9808";

void MCP9808Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MCP9808...");

  uint16_t manu;
  if (!this->read_byte_16(MCP9808_REG_MANUF_ID, &manu, 0) || manu != 0x0054) {
    this->mark_failed();
    ESP_LOGCONFIG(TAG, "MCP9808 manufacuturer id failed");
    return;
  }
  uint16_t dev_id;
  if (!this->read_byte_16(MCP9808_REG_DEVICE_ID, &dev_id, 0) || dev_id != 0x0400) {
    this->mark_failed();
    ESP_LOGCONFIG(TAG, "MCP9808 device id failed");
    return;
  }
}
void MCP9808Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP9808:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MCP9808 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_);
}
void MCP9808Component::update() {
  uint16_t raw_temp;
  if (!this->read_byte_16(MCP9808_REG_AMBIENT_TEMP, &raw_temp)) {
    this->status_set_warning();
    return;
  }
  if (raw_temp == 0xFFFF) {
    this->status_set_warning();
    return;
  }

  // http://ww1.microchip.com/downloads/en/DeviceDoc/25095A.pdf

  float temp = NAN;
  uint8_t msb = (uint8_t)((raw_temp & 0xff00) >> 8);
  uint8_t lsb = raw_temp & 0x00ff;

  msb = msb & 0x1F;  // clear flags
  if ((msb & 0x10) == 0x10) {
    msb = msb & 0x0F;  // clear sign
    temp = (256 - ((uint16_t)(msb) *16 + lsb / 16.0f)) * -1;
  } else {
    temp = (uint16_t)(msb) *16 + lsb / 16.0f;
  }

  if (temp == NAN) {
    this->status_set_warning();
    return;
  }

  this->temperature_->publish_state(temp);

  ESP_LOGD(TAG, "Got temperature=%.1f°C", temp);
  this->status_clear_warning();
}
float MCP9808Component::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace mcp9808
}  // namespace esphome
