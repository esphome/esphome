#ifdef USE_ESP_IDF

#include "esphome/core/defines.h"

#ifdef USE_MODEM
#ifdef USE_TEXT_SENSOR

#include "modem_text_sensor.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include "../modem_component.h"

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
namespace modem_text_sensor {

using namespace esp_modem;
// using namespace esphome::modem;

void ModemTextSensor::setup() { ESP_LOGI(TAG, "Setting up Modem Sensor..."); }

void ModemTextSensor::update() {
  ESP_LOGD(TAG, "Modem text_sensor update");
  if (modem::global_modem_component->dce && modem::global_modem_component->modem_ready()) {
    this->update_network_type_text_sensor_();
  }
}

void ModemTextSensor::update_network_type_text_sensor_() {
  if (modem::global_modem_component->modem_ready() && this->network_type_text_sensor_) {
    int act;
    std::string network_type = "Not available";
    if (modem::global_modem_component->dce->get_network_system_mode(act) == command_result::OK) {
      // Access Technology from AT+CNSMOD?
      // see https://www.waveshare.com/w/upload/a/af/SIM7500_SIM7600_Series_AT_Command_Manual_V3.00.pdf, page 109
      switch (act) {
        case 0:
          network_type = "No service";
          break;
        case 1:
          network_type = "GSM";
          break;
        case 2:
          network_type = "GPRS";
          break;
        case 3:
          network_type = "EGPRS (EDGE)";
          break;
        case 4:
          network_type = "WCDMA";
          break;
        case 5:
          network_type = "HSDPA only (WCDMA)";
          break;
        case 6:
          network_type = "HSUPA only (WCDMA)";
          break;
        case 7:
          network_type = "HSPA (HSDPA and HSUPA, WCDMA)";
          break;
        case 8:
          network_type = "LTE";
          break;
        case 9:
          network_type = "TDS-CDMA";
          break;
        case 10:
          network_type = "TDS-HSDPA only";
          break;
        case 11:
          network_type = "TDS-HSUPA only";
          break;
        case 12:
          network_type = "TDS-HSPA (HSDPA and HSUPA)";
          break;
        case 13:
          network_type = "CDMA";
          break;
        case 14:
          network_type = "EVDO";
          break;
        case 15:
          network_type = "HYBRID (CDMA and EVDO)";
          break;
        case 16:
          network_type = "1XLTE (CDMA and LTE)";
          break;
        case 23:
          network_type = "EHRPD";
          break;
        case 24:
          network_type = "HYBRID (CDMA and EHRPD)";
          break;
        default:
          network_type = "Unknown";
      }
    }
    this->network_type_text_sensor_->publish_state(network_type);
  }
}

}  // namespace modem_text_sensor
}  // namespace esphome

#endif  // USE_MODEM
#endif  // USE_TEXT_SENSOR
#endif  // USE_ESP_IDF
