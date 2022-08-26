#include "esphome/core/log.h"
#include "sml_sensor.h"
#include "../sml_parser.h"

namespace esphome {
namespace sml {

static const char *const TAG = "sml_sensor";

SmlSensor::SmlSensor(std::string server_id, std::string obis_code)
    : SmlListener(std::move(server_id), std::move(obis_code)) {}

void SmlSensor::publish_val(const ObisInfo &obis_info) {
  switch (obis_info.value_type) {
    case SML_INT: {
      publish_state(bytes_to_int(obis_info.value));
      break;
    }
    case SML_BOOL:
    case SML_UINT: {
      publish_state(bytes_to_uint(obis_info.value));
      break;
    }
    case SML_OCTET: {
      ESP_LOGW(TAG, "No number conversion for (%s) %s. Consider using SML TextSensor instead.",
               bytes_repr(obis_info.server_id).c_str(), obis_info.code_repr().c_str());
      break;
    }
  }
}

void SmlSensor::dump_config() {
  LOG_SENSOR("", "SML", this);
  if (!this->server_id.empty()) {
    ESP_LOGCONFIG(TAG, "  Server ID: %s", this->server_id.c_str());
  }
  ESP_LOGCONFIG(TAG, "  OBIS Code: %s", this->obis_code.c_str());
}

}  // namespace sml
}  // namespace esphome
