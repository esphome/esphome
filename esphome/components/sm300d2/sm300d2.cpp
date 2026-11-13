#include "sm300d2.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome::sm300d2 {

ESPHOME_LOG_TAG(TAG, "sm300d2");

void SM300D2Sensor::loop() {
  // The sensor sends frames on its own; read them as they arrive so the RX buffer does not overflow between polls
  this->read_available([this](uint8_t c) {
    const uint8_t *response = this->reader_.feed(c);
    if (response == nullptr)
      return;
    uint16_t calculated_checksum = sum_bytes(response, SM300D2_RESPONSE_LENGTH - 1);
    // Occasionally the checksum has a +/- 0x80 offset. Negative temperatures are
    // responsible for some of these. The rest are unknown/undocumented.
    if ((calculated_checksum != response[SM300D2_RESPONSE_LENGTH - 1]) &&
        (calculated_checksum - 0x80 != response[SM300D2_RESPONSE_LENGTH - 1]) &&
        (calculated_checksum + 0x80 != response[SM300D2_RESPONSE_LENGTH - 1])) {
      ESP_LOGV(TAG, "SM300D2 Checksum doesn't match: 0x%02X!=0x%02X", response[SM300D2_RESPONSE_LENGTH - 1],
               calculated_checksum);
      // The header may have been part of the payload of a misaligned frame, so realign instead of dropping everything
      this->reader_.resync();
      return;
    }
    memcpy(this->frame_, response, SM300D2_RESPONSE_LENGTH);
    this->has_frame_ = true;
  });
}

void SM300D2Sensor::update() {
  if (!this->has_frame_) {
    ESP_LOGW(TAG, "Reading data from SM300D2 failed!");
    this->status_set_warning();
    return;
  }
  this->has_frame_ = false;
  const uint8_t *response = this->frame_;

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
