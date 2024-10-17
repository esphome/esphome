#ifdef USE_ESP_IDF

#include "esphome/core/defines.h"

#ifdef USE_MODEM
#ifdef USE_TEXT_SENSOR

#include "modem_text_sensor.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include "../modem_component.h"
#include "../helpers.h"

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

using namespace esp_modem;
// using namespace esphome::modem;

static const char *const TAG = "modem.text_sensor";

void ModemTextSensor::setup() { ESP_LOGI(TAG, "Setting up Modem Sensor..."); }

void ModemTextSensor::update() {
  ESP_LOGD(TAG, "Modem text_sensor update");
  if (modem::global_modem_component->dce && modem::global_modem_component->modem_ready()) {
    this->update_network_type_text_sensor_();
    this->update_signal_strength_text_sensor_();
  }
}

void ModemTextSensor::update_network_type_text_sensor_() {
  if (modem::global_modem_component->modem_ready() && this->network_type_text_sensor_) {
    int act;
    std::string network_type = "Not available";
    if (modem::global_modem_component->dce->get_network_system_mode(act) == command_result::OK) {
      network_type = network_system_mode_to_string(act);
    }
    this->network_type_text_sensor_->publish_state(network_type);
  }
}

void ModemTextSensor::update_signal_strength_text_sensor_() {
  if (modem::global_modem_component->modem_ready() && this->signal_strength_text_sensor_) {
    float rssi, ber;
    if (modem::global_modem_component->get_signal_quality(rssi, ber)) {
      std::string bars = get_signal_bars(rssi, false);
      this->signal_strength_text_sensor_->publish_state(bars);
    }
  }
}

}  // namespace modem
}  // namespace esphome

#endif  // USE_MODEM
#endif  // USE_TEXT_SENSOR
#endif  // USE_ESP_IDF
