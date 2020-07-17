#include "t6615.h"
#include "esphome/core/log.h"

namespace esphome {
namespace t6615 {

static const char *TAG = "t6615";
static const uint8_t T6615_RESPONSE_BUFFER_LENGTH = 32;
static const uint8_t T6615_HEADER[] = {0xFF, 0xFE};
static const uint8_t T6615_COMMAND_GET_PPM[] = {0x02, 0x03};
static const uint8_t T6615_COMMAND_GET_SERIAL[] = {0x02, 0x01};
static const uint8_t T6615_COMMAND_GET_VERSION[] = {0x02, 0x0D};
static const uint8_t T6615_COMMAND_GET_ELEVATION[] = {0x02, 0x0F};
static const uint8_t T6615_COMMAND_GET_ABC[] = {0xB7, 0x00};
static const uint8_t T6615_COMMAND_ENABLE_ABC[] = {0xB7, 0x01};
static const uint8_t T6615_COMMAND_DISABLE_ABC[] = {0xB7, 0x02};
static const uint8_t T6615_COMMAND_SET_ELEVATION[] = {0x03, 0x0F};

void T6615Component::setup() {}

void T6615Component::update() {
  // CO2 sensor
  if (this->co2_sensor_ != nullptr) {
    uint16_t ppm = this->get_ppm_();
    if (ppm > 0) {
      ESP_LOGD(TAG, "T6615 Received CO₂=%uppm", ppm);
      this->co2_sensor_->publish_state(ppm);
    } else {
      ESP_LOGW(TAG, "Reading data from T6615 failed!");
      this->status_set_warning();
    }
  }
}

uint16_t T6615Component::get_ppm_() {
  uint8_t response[T6615_RESPONSE_BUFFER_LENGTH];
  uint8_t length = this->t6615_write_command_(sizeof(T6615_COMMAND_GET_PPM), T6615_COMMAND_GET_PPM, response);
  if (length == 0) {
    return 0;
  }

  const uint16_t ppm = (response[0] * 255) + response[1];
  return ppm;
}

uint8_t T6615Component::t6615_write_command_(uint8_t len, const uint8_t *command, uint8_t *response) {
  // Empty existing buffer
  while (this->available()) {
    this->read();
  }

  // Send command and wait for response
  this->write_array(T6615_HEADER, sizeof(T6615_HEADER));
  this->write_byte(len);
  this->write_array(command, len);
  this->flush();
  delay(40);

  // Read header
  uint8_t header[3];
  this->read_array(header, 3);
  if (header[0] != 0xFF || header[1] != 0xFA) {
    return 0;
  }

  // Read body
  this->read_array(response, header[2]);
  return header[2];
}

float T6615Component::get_setup_priority() const { return setup_priority::DATA; }
void T6615Component::dump_config() {
  ESP_LOGCONFIG(TAG, "T6615:");
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  this->check_uart_settings(19200);
}

}  // namespace t6615
}  // namespace esphome
