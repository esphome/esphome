#include "esphome/core/log.h"
#include "tuya_text_sensor.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.text_sensor";

int hexpair_to_int(std::string x, uint8 i)
{
    char a = x[i];
    char b = x[i+1];
    int value = 0;
    if      (a >= '0' && a <= '9') a =       (a - '0');
    else if (a >= 'A' && a <= 'F') a = (10 + (a - 'A'));
    else if (a >= 'a' && a <= 'f') a = (10 + (a - 'a'));
    if      (b >= '0' && b <= '9') b =       (b - '0');
    else if (b >= 'A' && b <= 'F') b = (10 + (b - 'A'));
    else if (b >= 'a' && b <= 'f') b = (10 + (b - 'a'));
    value = (a << 4) | b;
    return value;
}

int hexquad_to_int(std::string x, uint8 i)
{
    char a = x[i];
    char b = x[i+1];
    char c = x[i+2];
    char d = x[i+3];
    int value = 0;
    if      (a >= '0' && a <= '9') a =       (a - '0');
    else if (a >= 'A' && a <= 'F') a = (10 + (a - 'A'));
    else if (a >= 'a' && a <= 'f') a = (10 + (a - 'a'));
    if      (b >= '0' && b <= '9') b =       (b - '0');
    else if (b >= 'A' && b <= 'F') b = (10 + (b - 'A'));
    else if (b >= 'a' && b <= 'f') b = (10 + (b - 'a'));
    if      (c >= '0' && c <= '9') c =       (c - '0');
    else if (c >= 'A' && c <= 'F') c = (10 + (c - 'A'));
    else if (c >= 'a' && c <= 'f') c = (10 + (c - 'a'));
    if      (d >= '0' && d <= '9') d =       (d - '0');
    else if (d >= 'A' && d <= 'F') d = (10 + (d - 'A'));
    else if (d >= 'a' && d <= 'f') d = (10 + (d - 'a'));
    value = (a << 12) | (b << 8) | (c << 4) | d;
    return value;
}

std::vector<uint8_t> raw_decode(std::string x) 
{
    std::string res;
    for (int i = 0; i < x.size(); i=i+2) {
        res += hexpair_to_int(x,i);
    }
    std::vector<uint8_t> res_vec;
    res_vec.assign(res.begin(), res.end());
    return res_vec;
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
      this->publish_state(datapoint.value_string.c_str());
    } else if (datapoint.type == TuyaDatapointType::RAW) {
      this->publish_state(raw_encode(datapoint.value_raw).c_str());
    }
  });
}

void TuyaTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "Tuya Text Sensor", this);
  ESP_LOGCONFIG(TAG, "  Text Sensor has datapoint ID %u", this->sensor_id_);
}

}  // namespace tuya
}  // namespace esphome
