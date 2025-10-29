#include "cm1106.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace cm1106 {

static const char *const TAG = "cm1106";
static const uint8_t C_M1106_CMD_GET_CO2[4] = {0x11, 0x01, 0x01, 0xED};
static const uint8_t C_M1106_CMD_SET_CO2_CALIB[6] = {0x11, 0x03, 0x03, 0x00, 0x00, 0x00};
static const uint8_t C_M1106_CMD_SET_CO2_CALIB_RESPONSE[4] = {0x16, 0x01, 0x03, 0xE6};

uint8_t cm1106_checksum(const uint8_t *response, size_t len) {
  uint8_t crc = 0;
  for (size_t i = 0; i < len - 1; i++) {
    crc -= response[i];
  }
  return crc;
}

void CM1106Component::setup() {
  uint8_t response[8] = {0};
  if (!this->cm1106_write_command_(C_M1106_CMD_GET_CO2, sizeof(C_M1106_CMD_GET_CO2), response, sizeof(response))) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    this->mark_failed();
    return;
  }
}

void CM1106Component::update() {
  uint8_t response[8] = {0};
  if (!this->cm1106_write_command_(C_M1106_CMD_GET_CO2, sizeof(C_M1106_CMD_GET_CO2), response, sizeof(response))) {
    ESP_LOGW(TAG, "Reading data from CM1106 failed!");
    this->status_set_warning();
    return;
  }

  if (response[0] != 0x16 || response[1] != 0x05 || response[2] != 0x01) {
    ESP_LOGW(TAG, "Got wrong UART response from CM1106: %02X %02X %02X %02X", response[0], response[1], response[2],
             response[3]);
    this->status_set_warning();
    return;
  }

  uint8_t checksum = cm1106_checksum(response, sizeof(response));
  if (response[7] != checksum) {
    ESP_LOGW(TAG, "CM1106 Checksum doesn't match: 0x%02X!=0x%02X", response[7], checksum);
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();

  uint16_t ppm = response[3] << 8 | response[4];
  ESP_LOGD(TAG, "CM1106 Received CO₂=%uppm DF3=%02X DF4=%02X", ppm, response[5], response[6]);
  if (this->co2_sensor_ != nullptr)
    this->co2_sensor_->publish_state(ppm);
}

void CM1106Component::calibrate_zero(uint16_t ppm) {
  uint8_t cmd[6];
  memcpy(cmd, C_M1106_CMD_SET_CO2_CALIB, sizeof(cmd));
  cmd[3] = ppm >> 8;
  cmd[4] = ppm & 0xFF;
  uint8_t response[4] = {0};

  if (!this->cm1106_write_command_(cmd, sizeof(cmd), response, sizeof(response))) {
    ESP_LOGW(TAG, "Reading data from CM1106 failed!");
    this->status_set_warning();
    return;
  }

  // check if correct response received
  if (memcmp(response, C_M1106_CMD_SET_CO2_CALIB_RESPONSE, sizeof(response)) != 0) {
    ESP_LOGW(TAG, "Got wrong UART response from CM1106: %02X %02X %02X %02X", response[0], response[1], response[2],
             response[3]);
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();
  ESP_LOGD(TAG, "CM1106 Successfully calibrated sensor to %uppm", ppm);
}

bool CM1106Component::cm1106_write_command_(const uint8_t *command, size_t command_len, uint8_t *response,
                                            size_t response_len) {
  // Empty RX Buffer
  while (this->available())
    this->read();
  this->write_array(command, command_len - 1);
  this->write_byte(cm1106_checksum(command, command_len));
  this->flush();

  if (response == nullptr)
    return true;

  return this->read_array(response, response_len);
}

void CM1106Component::dump_config() {
  ESP_LOGCONFIG(TAG, "CM1106:");
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  this->check_uart_settings(9600);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

}  // namespace cm1106
}  // namespace esphome
