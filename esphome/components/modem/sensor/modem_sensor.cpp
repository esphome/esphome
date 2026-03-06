#ifdef USE_ESP32

#include "esphome/core/defines.h"

#ifdef USE_MODEM
#ifdef USE_SENSOR

#include "modem_sensor.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include "../modem_component.h"

#include <string>
#include <vector>
#include <sstream>
#include <map>

#define ESPHL_ERROR_CHECK(err, message) \
  if ((err) != ESP_OK) { \
    ESP_LOGE(TAG, message ": (%d) %s", err, esp_err_to_name(err)); \
    this->mark_failed(); \
    return; \
  }

#define ESPMODEM_ERROR_CHECK(err, message) \
  if ((err) != command_result::OK) { \
    ESP_LOGE(TAG, message ": %s", command_result_to_string(err).c_str()); \
  }

namespace esphome {
namespace modem {

static const char *const TAG = "modem.sensor";

using namespace esp_modem;

void ModemSensor::setup() { ESP_LOGI(TAG, "Setting up Modem Sensor..."); }

void ModemSensor::update() {
  if (global_modem_component->modem_handler->dce) {
    ESP_LOGV(TAG, "Modem sensor update");
    this->update_signal_sensors_();
  }
}

void ModemSensor::update_signal_sensors_() {
  if (this->rssi_sensor_ || this->ber_sensor_) {
    float rssi;
    float ber;
    global_modem_component->modem_handler->get_signal_quality(rssi, ber);
    if (this->rssi_sensor_)
      this->rssi_sensor_->publish_state(rssi);
    if (this->ber_sensor_)
      this->ber_sensor_->publish_state(ber);
  }
}

}  // namespace modem
}  // namespace esphome

#endif  // USE_MODEM
#endif  // USE_SENSOR
#endif  // USE_ESP32
