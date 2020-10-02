#include "mhz14a.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mhz14a {

static const char *TAG = "mhz14a";
static const uint8_t MHZ14A_REQUEST_LENGTH = 8;
static const uint8_t MHZ14A_RESPONSE_LENGTH = 9;
static const uint8_t MHZ14A_COMMAND_GET_PPM[] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x79};
static const uint8_t MHZ14A_COMMAND_CALIBRATE_ZERO[] = {0xFF, 0x01, 0x87, 0x00, 0x00, 0x00, 0x00, 0x78};

uint8_t mhz14a_checksum(const uint8_t *command) {
  uint8_t sum = 0;
  for (uint8_t i = 1; i < MHZ14A_REQUEST_LENGTH; i++) {
    sum += command[i];
  }
  return 0xFF - sum + 0x01;
}

void MHZ14AComponent::setup() {
}

void MHZ14AComponent::update() {
  uint8_t response[MHZ14A_RESPONSE_LENGTH];
  if (!this->mhz14a_write_command_(MHZ14A_COMMAND_GET_PPM, response)) {
    ESP_LOGW(TAG, "Reading data from MHZ14A failed!");
    this->status_set_warning();
    return;
  }

  if (response[0] != 0xFF || response[1] != 0x86) {
    ESP_LOGW(TAG, "Invalid preamble from MHZ14A!");
    this->status_set_warning();
    return;
  }

  uint8_t checksum = mhz14a_checksum(response);
  if (response[8] != checksum) {
    ESP_LOGW(TAG, "MHZ14A Checksum doesn't match: 0x%02X!=0x%02X", response[8], checksum);
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();
  const uint16_t ppm = (uint16_t(response[2]) << 8) | response[3];
  const int humidity = int(response[4]);
  const uint8_t status = response[5];

  ESP_LOGD(TAG, "MHZ14A Received CO₂=%uppm Humidity=%d%% Status=0x%02X", ppm, humidity, status);
  if (this->co2_sensor_ != nullptr)
    this->co2_sensor_->publish_state(ppm);
  if (this->humidity_sensor_ != nullptr)
    this->humidity_sensor_->publish_state(humidity);
}

void MHZ14AComponent::calibrate_zero() {
  ESP_LOGD(TAG, "MHZ14A Calibrating zero point");
  this->mhz14a_write_command_(MHZ14A_COMMAND_CALIBRATE_ZERO, nullptr);
}

bool MHZ14AComponent::mhz14a_write_command_(const uint8_t *command, uint8_t *response) {
  // Empty RX Buffer
  while (this->available())
    this->read();
  this->write_array(command, MHZ14A_REQUEST_LENGTH);
  this->write_byte(mhz14a_checksum(command));
  this->flush();

  if (response == nullptr)
    return true;

  return this->read_array(response, MHZ14A_RESPONSE_LENGTH);
}
float MHZ14AComponent::get_setup_priority() const { return setup_priority::DATA; }
void MHZ14AComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MH-Z14A:");
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  this->check_uart_settings(9600);
}

}  // namespace MHZ14A
}  // namespace esphome
