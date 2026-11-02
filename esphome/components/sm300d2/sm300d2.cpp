#include "sm300d2.h"
#include "esphome/core/frame_reader.h"
#include "esphome/core/log.h"

namespace esphome::sm300d2 {

ESPHOME_LOG_TAG(TAG, "sm300d2");
static constexpr uint8_t SM300D2_RESPONSE_LENGTH = 17;

void SM300D2Sensor::update() {
  // Takes the first frame already received; only one cut off at the end of the buffer is waited for
  FrameReader<SM300D2_RESPONSE_LENGTH, 0x3C, 0x02> reader;
  const uint8_t *response = nullptr;
  this->read_available<1>([&](uint8_t c) { return (response = reader.feed(c)) == nullptr; });
  for (uint8_t c; response == nullptr && reader.length() > 0 && this->read_byte(&c);)
    response = reader.feed(c);

  if (response == nullptr) {
    ESP_LOGW(TAG, "Reading data from SM300D2 failed!");
    this->status_set_warning();
    return;
  }

  uint16_t calculated_checksum = sum_bytes(response, SM300D2_RESPONSE_LENGTH - 1);
  // Occasionally the checksum has a +/- 0x80 offset. Negative temperatures are
  // responsible for some of these. The rest are unknown/undocumented.
  if ((calculated_checksum != response[SM300D2_RESPONSE_LENGTH - 1]) &&
      (calculated_checksum - 0x80 != response[SM300D2_RESPONSE_LENGTH - 1]) &&
      (calculated_checksum + 0x80 != response[SM300D2_RESPONSE_LENGTH - 1])) {
    ESP_LOGW(TAG, "SM300D2 Checksum doesn't match: 0x%02X!=0x%02X", response[SM300D2_RESPONSE_LENGTH - 1],
             calculated_checksum);
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();

  ESP_LOGD(TAG, "Successfully read SM300D2 data");

  const uint16_t co2 = (response[2] * 256) + response[3];
  const uint16_t formaldehyde = (response[4] * 256) + response[5];
  const uint16_t tvoc = (response[6] * 256) + response[7];
  const uint16_t pm_2_5 = (response[8] * 256) + response[9];
  const uint16_t pm_10_0 = (response[10] * 256) + response[11];
  // A negative value is indicated by adding 0x80 (128) to the temperature value
  const float temperature = ((response[12] + (response[13] * 0.1f)) > 128)
                                ? (((response[12] + (response[13] * 0.1f)) - 128) * -1)
                                : response[12] + (response[13] * 0.1f);
  const float humidity = response[14] + (response[15] * 0.1f);

  ESP_LOGD(TAG, "Received CO₂: %u ppm", co2);
  PUBLISH_STATE(this->co2_sensor_, co2);

  ESP_LOGD(TAG, "Received Formaldehyde: %u µg/m³", formaldehyde);
  PUBLISH_STATE(this->formaldehyde_sensor_, formaldehyde);

  ESP_LOGD(TAG, "Received TVOC: %u µg/m³", tvoc);
  PUBLISH_STATE(this->tvoc_sensor_, tvoc);

  ESP_LOGD(TAG, "Received PM2.5: %u µg/m³", pm_2_5);
  PUBLISH_STATE(this->pm_2_5_sensor_, pm_2_5);

  ESP_LOGD(TAG, "Received PM10: %u µg/m³", pm_10_0);
  PUBLISH_STATE(this->pm_10_0_sensor_, pm_10_0);

  ESP_LOGD(TAG, "Received Temperature: %.2f °C", temperature);
  PUBLISH_STATE(this->temperature_sensor_, temperature);

  ESP_LOGD(TAG, "Received Humidity: %.2f percent", humidity);
  PUBLISH_STATE(this->humidity_sensor_, humidity);
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
}

}  // namespace esphome::sm300d2
