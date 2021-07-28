#include "t6615.h"
#include "esphome/core/log.h"

namespace esphome {
namespace t6615 {

static const char *const TAG = "t6615";

static const uint8_t T6615_RESPONSE_BUFFER_LENGTH = 32;
static const uint8_t T6615_MAGIC = 0xFF;
static const uint8_t T6615_ADDR_HOST = 0xFA;
static const uint8_t T6615_ADDR_SENSOR = 0xFE;
static const uint8_t T6615_COMMAND_GET_PPM[] = {0x02, 0x03};
static const uint8_t T6615_COMMAND_GET_SERIAL[] = {0x02, 0x01};
static const uint8_t T6615_COMMAND_GET_VERSION[] = {0x02, 0x0D};
static const uint8_t T6615_COMMAND_GET_ELEVATION[] = {0x02, 0x0F};
static const uint8_t T6615_COMMAND_GET_ABC[] = {0xB7, 0x00};
static const uint8_t T6615_COMMAND_ENABLE_ABC[] = {0xB7, 0x01};
static const uint8_t T6615_COMMAND_DISABLE_ABC[] = {0xB7, 0x02};
static const uint8_t T6615_COMMAND_SET_ELEVATION[] = {0x03, 0x0F};

void T6615Component::loop() {
  if (!this->available())
    return;

  // Read header
  uint8_t header[3];
  this->read_array(header, 3);
  if (header[0] != T6615_MAGIC || header[1] != T6615_ADDR_HOST) {
    ESP_LOGW(TAG, "Reading data from T6615 failed!");
    while (this->available())
      this->read();  // Clear the incoming buffer
    this->status_set_warning();
    return;
  }

  // Read body
  uint8_t length = header[2];
  uint8_t response[T6615_RESPONSE_BUFFER_LENGTH];
  this->read_array(response, length);

  this->status_clear_warning();

  switch (this->command_) {
    case T6615Command::GET_PPM: {
      const uint16_t ppm = encode_uint16(response[0], response[1]);
      ESP_LOGD(TAG, "T6615 Received CO₂=%uppm", ppm);
      this->co2_sensor_->publish_state(ppm);
      break;
    }
    default:
      break;
  }

  this->command_ = T6615Command::NONE;
}

void T6615Component::update() { this->query_ppm_(); }

void T6615Component::query_ppm_() {
  if (this->co2_sensor_ == nullptr || this->command_ != T6615Command::NONE) {
    return;
  }

  this->command_ = T6615Command::GET_PPM;

  this->write_byte(T6615_MAGIC);
  this->write_byte(T6615_ADDR_SENSOR);
  this->write_byte(sizeof(T6615_COMMAND_GET_PPM));
  this->write_array(T6615_COMMAND_GET_PPM, sizeof(T6615_COMMAND_GET_PPM));
}

float T6615Component::get_setup_priority() const { return setup_priority::DATA; }
void T6615Component::dump_config() {
  ESP_LOGCONFIG(TAG, "T6615:");
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  this->check_uart_settings(19200);
}

}  // namespace t6615
}  // namespace esphome
