#include "tuya_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.text_sensor";

void TuyaTextSensor::setup() {
  this->parent_->register_listener(this->sensor_id_, [this](const TuyaDatapoint &datapoint) {
    switch (datapoint.type) {
      case TuyaDatapointType::STRING:
        ESP_LOGD(TAG, "MCU reported text sensor %u is: %s", datapoint.id, datapoint.value_string.c_str());
        this->publish_state(datapoint.value_string);
        break;
      case TuyaDatapointType::RAW: {
        std::string data = format_hex_pretty(datapoint.value_raw);
        ESP_LOGD(TAG, "MCU reported text sensor %u is: %s", datapoint.id, data.c_str());
        this->publish_state(data);
        break;
      }
      case TuyaDatapointType::ENUM: {
        char buf[4];  // uint8_t max is 3 digits + null
        snprintf(buf, sizeof(buf), "%u", datapoint.value_enum);
        ESP_LOGD(TAG, "MCU reported text sensor %u is: %s", datapoint.id, buf);
        this->publish_state(buf);
        break;
      }
      default:
        ESP_LOGW(TAG, "Unsupported data type for tuya text sensor %u: %#02hhX", datapoint.id, (uint8_t) datapoint.type);
        break;
    }
  });
}

void TuyaTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Tuya Text Sensor:\n"
                "  Text Sensor has datapoint ID %u",
                this->sensor_id_);
}

}  // namespace tuya
}  // namespace esphome
