// Implementation based on:
//  bb_temperature: https://github.com/bitbank2/bb_temperature
//  written by Larry Bank
//

#include "bb_temp.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "bb_temperature.h"

namespace esphome {
namespace bb_temp {

static const char *const TAG = "bb_temp";

void BBTempComponent::setup() {
  ESP_LOGE(TAG, "starting bb_temperature");
  int rc = _bbt.init(47, 48);  //_sda_pin, _scl_pin, false, 100000);
  if (rc != BBT_SUCCESS) {
    ESP_LOGE(TAG, "bb_temperature init() failed, rc = %d", rc);
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "bb_temperature init() succeeded!");
  _bbt.start();  // tell the sensor to start generating samples
} /* setup() */

void BBTempComponent::update() {
  _bbt.getSample(&_bbtSamp);

  if (this->temperature_sensor_ != nullptr) {
    float temperature = ((float) _bbtSamp.temperature) / 10.0f;
    this->temperature_sensor_->publish_state(temperature);
  }
  if (this->humidity_sensor_ != nullptr) {
    float humidity = (float) _bbtSamp.humidity;
    if (std::isnan(humidity)) {
      ESP_LOGW(TAG, "Invalid humidity reading (0%%), ");
    }
    this->humidity_sensor_->publish_state(humidity);
  }
  this->status_clear_warning();
} /* update() */

float BBTempComponent::get_setup_priority() const { return setup_priority::DATA; }

void BBTempComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "BB_TEMP:");
  //  LOG_I2C_DEVICE(this);
  //  if (this->is_failed()) {
  //    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  //  }
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

}  // namespace bb_temp
}  // namespace esphome
