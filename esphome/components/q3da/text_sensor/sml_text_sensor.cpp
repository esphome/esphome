#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "q3da_text_sensor.h"
#include "../q3da_parser.h"

namespace esphome {
namespace q3da {

static const char *const TAG = "q3da_text_sensor";

Q3DATextSensor::Q3DATextSensor(std::string server_id, std::string obis_code, Q3DAType format)
    : Q3DAListener(std::move(server_id), std::move(obis_code)), format_(format) {}

void Q3DATextSensor::publish_val(const ObisInfo &obis_info) {
  uint8_t value_type;
  if (this->format_ == Q3DA_UNDEFINED) {
    value_type = obis_info.value_type;
  } else {
    value_type = this->format_;
  }

  switch (value_type) {
    case Q3DA_HEX: {
      publish_state("0x" + bytes_repr(obis_info.value));
      break;
    }
    case Q3DA_INT: {
      publish_state(to_string(bytes_to_int(obis_info.value)));
      break;
    }
    case Q3DA_BOOL:
      publish_state(bytes_to_uint(obis_info.value) ? "True" : "False");
      break;
    case Q3DA_UINT: {
      publish_state(to_string(bytes_to_uint(obis_info.value)));
      break;
    }
    case Q3DA_OCTET: {
      publish_state(std::string(obis_info.value.begin(), obis_info.value.end()));
      break;
    }
  }
}

void Q3DATextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "Q3DA", this);
  if (!this->server_id.empty()) {
    ESP_LOGCONFIG(TAG, "  Server ID: %s", this->server_id.c_str());
  }
  ESP_LOGCONFIG(TAG, "  OBIS Code: %s", this->obis_code.c_str());
}

}  // namespace q3da
}  // namespace esphome
