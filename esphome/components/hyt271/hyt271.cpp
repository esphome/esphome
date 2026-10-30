#include "hyt271.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::hyt271 {

ESPHOME_LOG_TAG(TAG, "hyt271");

static constexpr uint32_t WAIT_CONVERT_TIMEOUT_ID = 0;

static const uint8_t HYT271_ADDRESS = 0x28;

void HYT271Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HYT271:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}
void HYT271Component::update() {
  uint8_t raw_data[4] = {0, 0, 0, 0};

  if (this->write(&raw_data[0], 0) != i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    return;
  }
  this->set_timeout(WAIT_CONVERT_TIMEOUT_ID, 50, [this]() {
    uint8_t raw_data[4];
    if (this->read(raw_data, 4) != i2c::ERROR_OK) {
      this->status_set_warning();
      ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
      return;
    }
    uint16_t raw_temperature = encode_uint16(raw_data[2], raw_data[3]) >> 2;
    uint16_t raw_humidity = encode_uint16(raw_data[0] & 0x3F, raw_data[1]);

    float temperature = ((float(raw_temperature)) * (165.0f / 16383.0f)) - 40.0f;
    float humidity = (float(raw_humidity)) * (100.0f / 16383.0f);

    ESP_LOGD(TAG, "Got Temperature=%.1f°C Humidity=%.1f%%", temperature, humidity);

    PUBLISH_STATE(this->temperature_sensor_, temperature);
    PUBLISH_STATE(this->humidity_sensor_, humidity);
    this->status_clear_warning();
  });
}
}  // namespace esphome::hyt271
