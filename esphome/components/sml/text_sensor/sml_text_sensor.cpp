#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "sml_text_sensor.h"
#include "../sml_parser.h"

namespace esphome {
namespace sml {

static const char *const TAG = "sml_text_sensor";

SmlTextSensor::SmlTextSensor(const std::string& server_id, const std::string& obis_code, SmlType format)
    : SmlListener(std::move(server_id), std::move(obis_code)), format_(format) {}

void SmlTextSensor::publish_val(const ObisInfo &obis_info) {
  uint8_t value_type;
  if (this->format_ == SML_UNDEFINED) {
    value_type = obis_info.value_type;
  } else {
    value_type = this->format_;
  }

  switch (value_type) {
    case SML_HEX: {
      publish_state("0x" + bytes_repr(obis_info.value));
      break;
    }
    case SML_INT: {
      publish_state(to_string(bytes_to_int(obis_info.value)));
      break;
    }
    case SML_BOOL:
      publish_state(bytes_to_uint(obis_info.value) ? "True" : "False");
      break;
    case SML_UINT: {
      publish_state(to_string(bytes_to_uint(obis_info.value)));
      break;
    }
    case SML_OCTET: {
      publish_state(std::string(obis_info.value.begin(), obis_info.value.end()));
      break;
    }
  }
}

void SmlTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "SML", this);
  if (!this->server_id.empty()) {
    ESP_LOGCONFIG(TAG, "  Server ID: %s", this->server_id.c_str());
  }
  ESP_LOGCONFIG(TAG, "  OBIS Code: %s", this->obis_code.c_str());
}

}  // namespace sml
}  // namespace esphome
