#include "hyt271.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace hyt271 {

static const char *const TAG = "hyt271";

static const uint8_t HYT271_ADDRESS = 0x28;

void HYT271Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HYT271:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
}
void HYT271Component::update() {
  uint8_t raw_data[4] = {0, 0, 0, 0};

  if (this->write(&raw_data[0], 0) != i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "Communication with HYT271 failed! => Ask new values");
    return;
  }
  this->set_timeout("wait_convert", 50, [this]() {
    uint8_t raw_data[4];
    if (this->read(raw_data, 4) != i2c::ERROR_OK) {
      this->status_set_warning();
      ESP_LOGE(TAG, "Communication with HYT271 failed! => Read values");
      return;
    }
    uint16_t raw_temperature = ((raw_data[2] << 8) | raw_data[3]) >> 2;
    uint16_t raw_humidity = ((raw_data[0] & 0x3F) << 8) | raw_data[1];

    float temperature = ((float(raw_temperature)) * (165.0f / 16383.0f)) - 40.0f;
    float humidity = (float(raw_humidity)) * (100.0f / 16383.0f);

    ESP_LOGD(TAG, "Got Temperature=%.1f°C Humidity=%.1f%%", temperature, humidity);

    if (this->temperature_ != nullptr)
      this->temperature_->publish_state(temperature);
    if (this->humidity_ != nullptr)
      this->humidity_->publish_state(humidity);
    this->status_clear_warning();
  });
}
float HYT271Component::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace hyt271
}  // namespace esphome
