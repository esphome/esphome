#include "sm300d2v2.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sm300d2v2 {
// Increased Byte legnth to read 19 vs old 17
static const char *const TAG = "sm300d2v2";
static const uint8_t SM300D2_RESPONSE_LENGTH = 19;

void SM300D2Sensor::update() {
  uint8_t response[SM300D2_RESPONSE_LENGTH];
  uint8_t peeked;
  uint8_t previous_byte = 0;
  // adding timer for trigger between datasets.
  unsigned long previous_time = 0;
  unsigned long start_time = millis();

  while (this->available() > 0) {
    this->peek_byte(&peeked);

    // Check if approx half-second delay has elapsed since the previous byte
    if (millis() - previous_time >= 300 && peeked == 0x01) {
      break;  // Exit the loop if the desired byte and delay are satisfied
    }

    previous_byte = peeked;
    previous_time = millis();
    this->read();
    // Reset the start time whenever a byte is registered
    start_time = millis();
  }
  // Reading in the 19 bytes into memory
  bool read_success = read_array(response, SM300D2_RESPONSE_LENGTH);
  // If no data recieved within 1 sec. Throw error
  if (!read_success) {
    ESP_LOGW(TAG, "Reading data from SM300D2 failed!");
    status_set_warning();
    return;
  }
  this->status_clear_warning();
  ESP_LOGD(TAG, "Successfully read SM300D2 data %s", response);
  // Bitchass Address of device
  const uint16_t addr = (response[0]);
  // Vendor Function Type
  const uint16_t function = (response[1]) + response[2];
  // Sensor Data
  const uint16_t co2 = (response[3] * 256) + response[4];
  const uint16_t formaldehyde = (response[5] * 256) + response[6];
  const uint16_t tvoc = (response[7] * 256) + response[8];
  const uint16_t pm_2_5 = (response[9] * 256) + response[10];
  const uint16_t pm_10_0 = (response[11] * 256) + response[12];
  // A negative value is indicated by adding 0x80 (128) to the temperature value
  const float temperature = ((response[13] + (response[14] * 0.1f)) > 128)
                                ? (((response[13] + (response[14] * 0.1f)) - 128) * -1)
                                : response[13] + (response[14] * 0.1f);
  const float humidity = response[15] + (response[14] * 0.1f);

  // Report out to log and publish to HA
  ESP_LOGD(TAG, "Received Addr: %u", addr);
  if (this->addr_sensor_ != nullptr)
    this->addr_sensor_->publish_state(addr);
  ESP_LOGD(TAG, "Received Function Type: %u", function);
  if (this->function_sensor_ != nullptr)
    this->function_sensor_->publish_state(function);
  ESP_LOGD(TAG, "Received CO₂: %u ppm", co2);
  if (this->co2_sensor_ != nullptr)
    this->co2_sensor_->publish_state(co2);
  ESP_LOGD(TAG, "Received Formaldehyde: %u µg/m³", formaldehyde);
  if (this->formaldehyde_sensor_ != nullptr)
    this->formaldehyde_sensor_->publish_state(formaldehyde);
  ESP_LOGD(TAG, "Received TVOC: %u µg/m³", tvoc);
  if (this->tvoc_sensor_ != nullptr)
    this->tvoc_sensor_->publish_state(tvoc);
  ESP_LOGD(TAG, "Received PM2.5: %u µg/m³", pm_2_5);
  if (this->pm_2_5_sensor_ != nullptr)
    this->pm_2_5_sensor_->publish_state(pm_2_5);
  ESP_LOGD(TAG, "Received PM10: %u µg/m³", pm_10_0);
  if (this->pm_10_0_sensor_ != nullptr)
    this->pm_10_0_sensor_->publish_state(pm_10_0);
  ESP_LOGD(TAG, "Received Temperature: %.2f °C", temperature);
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);
  ESP_LOGD(TAG, "Received Humidity: %.2f percent", humidity);
  if (this->humidity_sensor_ != nullptr)
    this->humidity_sensor_->publish_state(humidity);
}

void SM300D2Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "SM300D2:");
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "Formaldehyde", this->formaldehyde_sensor_);
  LOG_SENSOR("  ", "TVOC", this->tvoc_sensor_);
  LOG_SENSOR("  ", "PM2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM10", this->pm_10_0_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  this->check_uart_settings(9600);
}

}  // namespace sm300d2v2
}  // namespace esphome
