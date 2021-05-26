#include "esphome/core/log.h"
#include "tuya_text_sensor.h"

namespace esphome {
namespace tuya {

static const char *TAG = "tuya.text_sensor";

void TuyaTextSensor::setup() {
  this->parent_->register_listener(this->sensor_id_, [this](TuyaDatapoint datapoint) {
    switch (datapoint.type) {
      case TuyaDatapointType::STRING:
        ESP_LOGD(TAG, "MCU reported text sensor is: %s", datapoint.value_string.c_str());
        this->publish_state(datapoint.value_string);
        break;
      case TuyaDatapointType::RAW: {
        // wait until #1669 merged to dev branch
        // std::string data = rawencode(datapoint.value_raw.data(), datapoint.value_raw.size());
        std::string data = datapoint.value_string;
        ESP_LOGD(TAG, "MCU reported text sensor is: %s", data.c_str());
        this->publish_state(data);
        break;
      }
      default:
        ESP_LOGW(TAG, "Unsupported data type for tuya text sensor: %hhu", datapoint.type);
        break;
    }
  });
}

void TuyaTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya Text Sensor:");
  ESP_LOGCONFIG(TAG, "  Text Sensor has datapoint ID %u", this->sensor_id_);
}

}  // namespace tuya
}  // namespace esphome
