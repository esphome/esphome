#include "ams5915.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ams5915 {
static const char *const TAG = "ams5915";
AMS5915 sPress(Wire,0x28,AMS5915::AMS5915_1000_D_B);
void Ams5915::setup() {
  if (sPress.begin() < 0) {
    ESP_LOGE(TAG, "Failed to read pressure from AMS5915");
    this->mark_failed();
  }
}

void Ams5915::update() {
  sPress.readSensor();
  float temperature = sPress.getTemperature_C();
  float pressure = sPress.getPressure_Pa();


  ESP_LOGD(TAG, "Got temperature=%.1f°C pressure=%.1fpa", temperature, pressure);
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);
  if (this->pressure_sensor_ != nullptr)
    this->pressure_sensor_->publish_state(pressure);
}


void Ams5915::dump_config() {
  ESP_LOGCONFIG(TAG, "ams5915:");
  LOG_I2C_DEVICE(this);
}


}  // namespace ams5915
}  // namespace esphome
