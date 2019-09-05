#include "senseair.h"
#include "esphome/core/log.h"

namespace esphome {
namespace senseair {

static const char *TAG = "senseair";
static const uint8_t SENSEAIR_REQUEST_LENGTH = 8;
static const uint8_t SENSEAIR_RESPONSE_LENGTH = 7;
static const uint8_t SENSEAIR_COMMAND_GET_PPM[] = {0xFE, 0x04, 0x00, 0x03, 0x00, 0x01, 0xD5, 0xC5};

void SenseAirComponent::update() {
  uint8_t response[SENSEAIR_RESPONSE_LENGTH];
  if (!this->senseair_write_command_(SENSEAIR_COMMAND_GET_PPM, response)) {
    ESP_LOGW(TAG, "Reading data from SenseAir failed!");
    this->status_set_warning();
    return;
  }

  if (response[0] != 0xFE || response[1] != 0x04) {
    ESP_LOGW(TAG, "Invalid preamble from SenseAir!");
    this->status_set_warning();
    return;
  }

  uint16_t checksum = this->senseair_checksum_(response, 5);
  if (((response[6] << 8) | response[5]) != checksum) {
    ESP_LOGW(TAG, "SenseAir checksum doesn't match: 0x%02X!=0x%02X", response[6], checksum);
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();
  const uint16_t ppm = (uint16_t(response[3]) << 8) | response[4];

  ESP_LOGD(TAG, "SenseAir Received CO₂=%uppm", ppm);
  if (this->co2_sensor_ != nullptr)
    this->co2_sensor_->publish_state(ppm);
}

uint16_t SenseAirComponent::senseair_checksum_(uint8_t *ptr, uint8_t length) {
  uint16_t crc = 0xFFFF;
  uint8_t i;
  while (length--) {
    crc ^= *ptr++;
    for (i = 0; i < 8; i++) {
      if ((crc & 0x01) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

bool SenseAirComponent::senseair_write_command_(const uint8_t *command, uint8_t *response) {
  this->flush();
  this->write_array(command, SENSEAIR_REQUEST_LENGTH);

  if (response == nullptr)
    return true;

  bool ret = this->read_array(response, SENSEAIR_RESPONSE_LENGTH);
  this->flush();
  return ret;
}

void SenseAirComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SenseAir:");
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
}

}  // namespace senseair
}  // namespace esphome
