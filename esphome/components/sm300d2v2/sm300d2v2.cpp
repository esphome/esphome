#include "sm300d2v2.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sm300d2v2 {

static const char *const TAG = "sm300d2v2";
static const uint8_t SM300D2_RESPONSE_LENGTH = 19;

void SM300D2Sensor::update() {
  uint8_t response[SM300D2_RESPONSE_LENGTH];
  uint8_t peeked;

  while (this->available() > 0 && this->peek_byte(&peeked) && peeked != 0x01)
    this->read();

// Trim 3 characters from the beginning of the response array
  bool read_success = read_array(response, SM300D2_RESPONSE_LENGTH);

  string response = SM300D2_RESPONSE_LENGTH.Substring(3);
  Console.WriteLine(response);

  if (!read_success) {
    ESP_LOGW(TAG, "Reading data from SM300D2 failed!");
    status_set_warning();
    return;
  }

  //if (response[0] != 0x3C || response[1] != 0x02) {
  //  ESP_LOGW(TAG, "Invalid preamble for SM300D2 response!");
  //  this->status_set_warning();
  //  return;
  //}

 // uint16_t calculated_checksum = this->sm300d2_checksum_(response);
  // Occasionally the checksum has a +/- 0x80 offset. Negative temperatures are
  // responsible for some of these. The rest are unknown/undocumented.
 // if ((calculated_checksum != response[SM300D2_RESPONSE_LENGTH - 1]) &&
  //    (calculated_checksum - 0x80 != response[SM300D2_RESPONSE_LENGTH - 1]) &&
  //    (calculated_checksum + 0x80 != response[SM300D2_RESPONSE_LENGTH - 1])) {
  //  ESP_LOGW(TAG, "SM300D2 Checksum doesn't match: 0x%02X!=0x%02X", response[SM300D2_RESPONSE_LENGTH - 1],
  //           calculated_checksum);
  //  this->status_set_warning();
  //  return;
 // }

  this->status_clear_warning();

  ESP_LOGD(TAG, "Successfully read SM300D2 data");

  const uint16_t co2 = (response[1] * 256) + response[2];
  const uint16_t formaldehyde = (response[3] * 256) + response[4];
  const uint16_t tvoc = (response[5] * 256) + response[6];
  const uint16_t pm_2_5 = (response[7] * 256) + response[8];
  const uint16_t pm_10_0 = (response[9] * 256) + response[10];
  // A negative value is indicated by adding 0x80 (128) to the temperature value
  const float temperature = ((response[11] + (response[12] * 0.1f)) > 128)
                                ? (((response[11] + (response[12] * 0.1f)) - 128) * -1)
                                : response[11] + (response[12] * 0.1f);
  const float humidity = response[13] + (response[14] * 0.1f);

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

uint16_t SM300D2Sensor::sm300d2_checksum_(uint8_t *ptr) {
  uint8_t sum = 0;
  for (int i = 0; i < (SM300D2_RESPONSE_LENGTH - 1); i++) {
    sum += *ptr++;
  }
  return sum;
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

}  // namespace sm300d2
}  // namespace esphome
