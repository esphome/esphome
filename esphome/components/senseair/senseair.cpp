#include "senseair.h"
#include "esphome/core/log.h"

namespace esphome {
namespace senseair {

static const char *TAG = "senseair";
static const uint8_t SENSEAIR_REQUEST_LENGTH = 8;
static const uint8_t SENSEAIR_PPM_STATUS_RESPONSE_LENGTH = 13;
static const uint8_t SENSEAIR_ABC_PERIOD_RESPONSE_LENGTH = 7;
static const uint8_t SENSEAIR_CAL_RESULT_RESPONSE_LENGTH = 7;
static const uint8_t SENSEAIR_COMMAND_GET_PPM_STATUS[] = {0xFE, 0x04, 0x00, 0x00, 0x00, 0x04, 0xE5, 0xC6};
static const uint8_t SENSEAIR_COMMAND_CLEAR_ACK_REGISTER[] = {0xFE, 0x06, 0x00, 0x00, 0x00, 0x00, 0x9D, 0xC5};
static const uint8_t SENSEAIR_COMMAND_BACKGROUND_CAL[] = {0xFE, 0x06, 0x00, 0x01, 0x7C, 0x06, 0x6C, 0xC7};
static const uint8_t SENSEAIR_COMMAND_BACKGROUND_CAL_RESULT[] = {0xFE, 0x03, 0x00, 0x00, 0x00, 0x01, 0x90, 0x05};
static const uint8_t SENSEAIR_COMMAND_ABC_ENABLE[] = {0xFE, 0x06, 0x00, 0x1F, 0x00, 0xB4, 0xAC, 0x74};  // 180 hours
static const uint8_t SENSEAIR_COMMAND_ABC_DISABLE[] = {0xFE, 0x06, 0x00, 0x1F, 0x00, 0x00, 0xAC, 0x03};
static const uint8_t SENSEAIR_COMMAND_ABC_GET_PERIOD[] = {0xFE, 0x03, 0x00, 0x1F, 0x00, 0x01, 0xA1, 0xC3};

void SenseAirComponent::update() {
  uint8_t response[SENSEAIR_PPM_STATUS_RESPONSE_LENGTH];
  if (!this->senseair_write_command_(SENSEAIR_COMMAND_GET_PPM_STATUS, response, SENSEAIR_PPM_STATUS_RESPONSE_LENGTH)) {
    ESP_LOGW(TAG, "Reading data from SenseAir failed!");
    this->status_set_warning();
    return;
  }

  if (response[0] != 0xFE || response[1] != 0x04) {
    ESP_LOGW(TAG, "Invalid preamble from SenseAir! %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x",
             response[0], response[1], response[2], response[3], response[4], response[5], response[6], response[7],
             response[8], response[9], response[10], response[11], response[12]);

    this->status_set_warning();
    while (this->available()) {
      uint8_t b;
      if (this->read_byte(&b)) {
        ESP_LOGV(TAG, "    ... %02x", b);
      } else {
        ESP_LOGV(TAG, "    ... nothing read");
      }
    }
    return;
  }

  uint16_t calc_checksum = this->senseair_checksum_(response, 11);
  uint16_t resp_checksum = (uint16_t(response[12]) << 8) | response[11];
  if (resp_checksum != calc_checksum) {
    ESP_LOGW(TAG, "SenseAir checksum doesn't match: 0x%02X!=0x%02X", resp_checksum, calc_checksum);
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();
  const uint8_t length = response[2];
  const uint16_t status = (uint16_t(response[3]) << 8) | response[4];
  const uint16_t ppm = (uint16_t(response[length + 1]) << 8) | response[length + 2];

  ESP_LOGD(TAG, "SenseAir Received CO₂=%uppm Status=0x%02X", ppm, status);
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

void SenseAirComponent::background_calibration() {
  ESP_LOGD(TAG, "SenseAir Starting background calibration");
  this->senseair_write_command_(SENSEAIR_COMMAND_CLEAR_ACK_REGISTER, nullptr, 0);
  this->senseair_write_command_(SENSEAIR_COMMAND_BACKGROUND_CAL, nullptr, 0);
}

void SenseAirComponent::background_calibration_result() {
  ESP_LOGD(TAG, "SenseAir Requesting background calibration result");
  uint8_t response[SENSEAIR_CAL_RESULT_RESPONSE_LENGTH];
  if (!this->senseair_write_command_(SENSEAIR_COMMAND_BACKGROUND_CAL_RESULT, response,
                                     SENSEAIR_CAL_RESULT_RESPONSE_LENGTH)) {
    ESP_LOGE(TAG, "Requesting background calibration result from SenseAir failed!");
    return;
  }

  if (response[0] != 0xFE || response[1] != 0x03) {
    ESP_LOGE(TAG, "Invalid reply from SenseAir! %02x%02x%02x %02x%02x %02x%02x", response[0], response[1], response[2],
             response[3], response[4], response[5], response[6]);
    return;
  }

  ESP_LOGD(TAG, "SenseAir Result=%s (%02x%02x%02x)", response[2] == 2 ? "OK" : "NOT_OK", response[2], response[3],
           response[4]);
}

void SenseAirComponent::abc_enable() {
  ESP_LOGD(TAG, "SenseAir Enabling automatic baseline calibration");
  this->senseair_write_command_(SENSEAIR_COMMAND_ABC_ENABLE, nullptr, 0);
}

void SenseAirComponent::abc_disable() {
  ESP_LOGD(TAG, "SenseAir Disabling automatic baseline calibration");
  this->senseair_write_command_(SENSEAIR_COMMAND_ABC_DISABLE, nullptr, 0);
}

void SenseAirComponent::abc_get_period() {
  ESP_LOGD(TAG, "SenseAir Requesting ABC period");
  uint8_t response[SENSEAIR_ABC_PERIOD_RESPONSE_LENGTH];
  if (!this->senseair_write_command_(SENSEAIR_COMMAND_ABC_GET_PERIOD, response, SENSEAIR_ABC_PERIOD_RESPONSE_LENGTH)) {
    ESP_LOGE(TAG, "Requesting ABC period from SenseAir failed!");
    return;
  }

  if (response[0] != 0xFE || response[1] != 0x03) {
    ESP_LOGE(TAG, "Invalid reply from SenseAir! %02x%02x%02x %02x%02x %02x%02x", response[0], response[1], response[2],
             response[3], response[4], response[5], response[6]);
    return;
  }

  const uint16_t hours = (uint16_t(response[3]) << 8) | response[4];
  ESP_LOGD(TAG, "SenseAir Read ABC Period: %u hours", hours);
}

bool SenseAirComponent::senseair_write_command_(const uint8_t *command, uint8_t *response, uint8_t response_length) {
  this->flush();
  this->write_array(command, SENSEAIR_REQUEST_LENGTH);

  if (response == nullptr)
    return true;

  bool ret = this->read_array(response, response_length);
  this->flush();
  return ret;
}

void SenseAirComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SenseAir:");
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  this->check_uart_settings(9600);
}

}  // namespace senseair
}  // namespace esphome
