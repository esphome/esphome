#include "senseair.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace senseair {

static const char *const TAG = "senseair";
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

  uint16_t calc_checksum = crc16(response, 11);
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

void SenseAirComponent::background_calibration() {
  // Responses are just echoes but must be read to clear the buffer
  ESP_LOGD(TAG, "SenseAir Starting background calibration");
  uint8_t command_length = sizeof(SENSEAIR_COMMAND_CLEAR_ACK_REGISTER) / sizeof(SENSEAIR_COMMAND_CLEAR_ACK_REGISTER[0]);
  uint8_t response[command_length];
  this->senseair_write_command_(SENSEAIR_COMMAND_CLEAR_ACK_REGISTER, response, command_length);
  this->senseair_write_command_(SENSEAIR_COMMAND_BACKGROUND_CAL, response, command_length);
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

  // Check if 5th bit (register CI6) is set
  ESP_LOGI(TAG, "SenseAir Result=%s (%02x%02x%02x %02x%02x %02x%02x)", (response[4] & 0b100000) != 0 ? "OK" : "NOT_OK",
           response[0], response[1], response[2], response[3], response[4], response[5], response[6]);
}

void SenseAirComponent::abc_enable() {
  // Response is just an echo but must be read to clear the buffer
  ESP_LOGD(TAG, "SenseAir Enabling automatic baseline calibration");
  uint8_t command_length = sizeof(SENSEAIR_COMMAND_ABC_ENABLE) / sizeof(SENSEAIR_COMMAND_ABC_ENABLE[0]);
  uint8_t response[command_length];
  this->senseair_write_command_(SENSEAIR_COMMAND_ABC_ENABLE, response, command_length);
}

void SenseAirComponent::abc_disable() {
  // Response is just an echo but must be read to clear the buffer
  ESP_LOGD(TAG, "SenseAir Disabling automatic baseline calibration");
  uint8_t command_length = sizeof(SENSEAIR_COMMAND_ABC_DISABLE) / sizeof(SENSEAIR_COMMAND_ABC_DISABLE[0]);
  uint8_t response[command_length];
  this->senseair_write_command_(SENSEAIR_COMMAND_ABC_DISABLE, response, command_length);
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
  // Verify we have somewhere to store the response
  if (response == nullptr) {
    return false;
  }
  // Write wake up byte required by some S8 sensor models
  this->write_byte(0);
  this->flush();
  delay(5);
  this->write_array(command, SENSEAIR_REQUEST_LENGTH);

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
