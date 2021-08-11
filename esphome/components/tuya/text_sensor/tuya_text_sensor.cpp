#include "esphome/core/log.h"
#include "tuya_text_sensor.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.text_sensor";

uint8_t hexchar_to_int(const std::string &dp_data_string, const uint8_t index_in_string) {
  uint8_t out = dp_data_string[index_in_string];
  if (out >= '0' && out <= '9')
    out = (out - '0');
  else if (out >= 'A' && out <= 'F')
    out = (10 + (out - 'A'));
  else if (out >= 'a' && out <= 'f')
    out = (10 + (out - 'a'));
  return out;
}

uint8_t hexpair_to_int(const std::string &dp_data_string, const uint8_t index_in_string) {
  uint8_t a = hexchar_to_int(dp_data_string, index_in_string);
  uint8_t b = hexchar_to_int(dp_data_string, index_in_string + 1);
  return (a << 4) | b;
}

uint16_t hexquad_to_int(const std::string &dp_data_string, const uint8_t index_in_string) {
  uint8_t a = hexchar_to_int(dp_data_string, index_in_string);
  uint8_t b = hexchar_to_int(dp_data_string, index_in_string + 1);
  uint8_t c = hexchar_to_int(dp_data_string, index_in_string + 2);
  uint8_t d = hexchar_to_int(dp_data_string, index_in_string + 3);
  return (a << 12) | (b << 8) | (c << 4) | d;
}

std::vector<uint8_t> raw_decode(const std::string &dp_data_string) {
  std::vector<uint8_t> res;
  res.resize(dp_data_string.size() / 2); // reserve enough memory for all bytes
  for(size_t i = 0; i < dp_data_string.size(); i += 2)
    res.push_back(hexpair_to_int(dp_data_string, i));
  return res;
}

std::string raw_encode(const uint8_t *data, uint32_t len) {
  char buf[20];
  std::string res;
  for (size_t i = 0; i < len; i++) {
    if (i + 1 != len) {
      sprintf(buf, "%02X", data[i]);
    } else {
      sprintf(buf, "%02X", data[i]);
    }
    res += buf;
  }
  return res;
}

void TuyaTextSensor::setup() {
  this->parent_->register_listener(this->sensor_id_, [this](const TuyaDatapoint &datapoint) {
    if (datapoint.type == TuyaDatapointType::STRING) {
      ESP_LOGV(TAG, "MCU reported text sensor %u is (string): %s", datapoint.id, datapoint.value_string.c_str());
      this->publish_state(datapoint.value_string);
    } else if (datapoint.type == TuyaDatapointType::RAW) {
      ESP_LOGV(TAG, "MCU reported text sensor %u is (raw as string): %s", datapoint.id,
               raw_encode(datapoint.value_raw).c_str());
      this->publish_state(raw_encode(datapoint.value_raw));
    }
  });
}

void TuyaTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "Tuya Text Sensor", this);
  ESP_LOGCONFIG(TAG, "  Text Sensor has datapoint ID %u", this->sensor_id_);
}

}  // namespace tuya
}  // namespace esphome
